#pragma once

#include "ModelId.hpp"

#include <limits>
#include <ranges>
#include <string_view>
#include <vector>

namespace Model
{

struct SqlColumnIndex
{
    size_t value;
};

enum class SqlWhereOperator : uint8_t
{
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    GREATER_THAN,
    LESS_OR_EQUAL,
    GREATER_OR_EQUAL
};

// Base class for every SqlModel<T>.
struct SqlModelBase
{
  public:
    SqlModelBase(std::string_view tableName, std::string_view primaryKey, SqlModelId id):
        m_tableName { tableName },
        m_primaryKeyName { primaryKey },
        m_id { id }
    {
    }

    SqlModelBase(SqlModelBase&& other) noexcept:
        m_tableName { other.m_tableName },
        m_primaryKeyName { other.m_primaryKeyName },
        m_id { other.m_id }
    {
    }

    SqlModelBase() = delete;
    SqlModelBase(SqlModelBase const&) = delete;
    SqlModelBase& operator=(SqlModelBase const&) = delete;
    SqlModelBase& operator=(SqlModelBase&&) = default;
    ~SqlModelBase() = default;

    // clang-format off
    std::string_view TableName() const noexcept { return m_tableName; }
    std::string_view PrimaryKeyName() const noexcept { return m_primaryKeyName; }
    SqlModelId Id() const noexcept { return m_id; }

    void RegisterField(SqlModelFieldBase& field) noexcept { m_fields.push_back(&field); }

    void UnregisterField(SqlModelFieldBase const& field) noexcept
    {
        if (auto it = std::find(m_fields.begin(), m_fields.end(), &field); it != m_fields.end())
            m_fields.erase(it);
    }

    void RegisterRelation(SqlModelRelation& relation) noexcept { m_relations.push_back(&relation); }

    SqlModelFieldBase const& GetField(SqlColumnIndex index) const noexcept { return *m_fields[index.value]; }
    SqlModelFieldBase& GetField(SqlColumnIndex index) noexcept { return *m_fields[index.value]; }
    // clang-format on

    void SetModified(bool value) noexcept
    {
        for (auto* field: m_fields)
            field->SetModified(value);
    }

    bool IsModified() const noexcept
    {
        return std::ranges::any_of(m_fields, [](SqlModelFieldBase* field) { return field->IsModified(); });
    }

    void SortFieldsByIndex() noexcept
    {
        std::sort(m_fields.begin(), m_fields.end(), [](auto a, auto b) { return a->Index() < b->Index(); });
    }

    using FieldList = std::vector<SqlModelFieldBase*>;

    FieldList GetModifiedFields() const noexcept
    {
        FieldList result;
        std::ranges::copy_if(m_fields, std::back_inserter(result), [](auto* field) { return field->IsModified(); });
        return result;
    }

  protected:
    std::string_view m_tableName;      // Should be const, but we want to allow move semantics
    std::string_view m_primaryKeyName; // Should be const, but we want to allow move semantics
    SqlModelId m_id {};

    bool m_modified = false;
    FieldList m_fields;
    std::vector<SqlModelRelation*> m_relations;
};

template <typename Derived>
struct SqlModel: public SqlModelBase
{
    // Returns a human readable string representation of this model.
    std::string Inspect() const noexcept;

    // Creates (or recreates a copy of) the model in the database.
    SqlResult<SqlModelId> Create();

    // Reads the model from the database by given model ID.
    SqlResult<void> Load(SqlModelId id);

    // Updates the model in the database.
    SqlResult<void> Update();

    // Creates or updates the model in the database, depending on whether it already exists.
    SqlResult<void> Save();

    // Deletes the model from the database.
    SqlResult<void> Destroy();

    // Updates all models with the given changes in the modelChanges model.
    static SqlResult<void> UpdateAll(Derived const& modelChanges) noexcept;

    // Retrieves the first model from the database (ordered by ID ASC).
    static SqlResult<Derived> First();

    // Retrieves the last model from the database (ordered by ID ASC).
    static SqlResult<Derived> Last();

    // Retrieves the model with the given ID from the database.
    static SqlResult<Derived> Find(SqlModelId id);

    template <typename ColumnName, typename T>
    static SqlResult<Derived> FindBy(ColumnName const& columnName, T const& value) noexcept;

    // Retrieves all models of this kind from the database.
    static SqlResult<std::vector<Derived>> All() noexcept;

    // Retrieves the number of models of this kind from the database.
    static SqlResult<size_t> Count() noexcept;

    static SqlResult<std::vector<SqlModel>> Where(SqlColumnIndex columnIndex,
                                                  const SqlVariant& value,
                                                  SqlWhereOperator whereOperator = SqlWhereOperator::EQUAL) noexcept;

    static SqlResult<std::vector<SqlModel>> Where(const std::string& columnName,
                                                  const SqlVariant& value,
                                                  SqlWhereOperator whereOperator = SqlWhereOperator::EQUAL) noexcept;

    // Returns the SQL string to create the table for this model.
    static std::string CreateTableString(SqlServerType serverType) noexcept;

    // Creates the table for this model in the database.
    static SqlResult<void> CreateTable() noexcept;

  protected:
    explicit SqlModel(std::string_view tableName, std::string_view primaryKey = "id");
};

#pragma region SqlModel<> implementation

template <typename Derived>
SqlModel<Derived>::SqlModel(std::string_view tableName, std::string_view primaryKey):
    SqlModelBase { tableName, primaryKey, SqlModelId {} }
{
}

template <typename Derived>
SqlResult<size_t> SqlModel<Derived>::Count() noexcept
{
    SqlStatement stmt;
    Derived modelSchema;

    return stmt.Prepare(std::format("SELECT COUNT(*) FROM {}", modelSchema.m_tableName))
        .and_then([&] { return stmt.Execute(); })
        .and_then([&] { return stmt.FetchRow(); })
        .and_then([&] { return stmt.GetColumn<size_t>(1); });
}

template <typename Derived>
SqlResult<Derived> SqlModel<Derived>::Find(SqlModelId id)
{
    Derived model;
    model.Load(id);
    std::println("Loaded model: {}", model.Inspect());
    return { std::move(model) };
}

template <typename Derived>
SqlResult<std::vector<Derived>> SqlModel<Derived>::All() noexcept
{
    std::vector<Derived> allModels;

    Derived model;

    detail::StringBuilder sqlColumnsString;
    sqlColumnsString << model.m_primaryKeyName;
    for (SqlModelFieldBase const* field: model.m_fields)
        sqlColumnsString << ", " << field->Name();

    SqlStatement stmt;

    if (auto result = stmt.Prepare(std::format("SELECT {} FROM {}", *sqlColumnsString, model.m_tableName)); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.BindOutputColumn(1, &model.m_id.value); !result)
        return std::unexpected { result.error() };

    for (SqlModelFieldBase* field: model.m_fields)
        if (auto result = field->BindOutputColumn(stmt); !result)
            return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    while (stmt.FetchRow())
    {
        std::println("Fetching row: {}", model.Inspect());
        allModels.emplace_back(model);
    }

    if (stmt.LastError() != SqlError::NO_DATA_FOUND)
        return std::unexpected { stmt.LastError() };

    return { std::move(allModels) };
}

template <typename Derived>
std::string SqlModel<Derived>::Inspect() const noexcept
{
    detail::StringBuilder result;

    // Reserve enough space for the output string (This is merely a guess, but it's better than nothing)
    result.output.reserve(m_tableName.size() + m_fields.size() * 32);

    result << "#<" << m_tableName << " id=" << m_id.value;
    for (auto const* field: m_fields)
        result << ", " << field->Name() << "=" << field->InspectValue();
    result << ">";

    return std::move(*result);
}

template <typename Derived>
std::string SqlModel<Derived>::CreateTableString(SqlServerType serverType) noexcept
{
    SqlTraits const& traits = GetSqlTraits(serverType); // TODO: take server type from connection
    detail::StringBuilder sql;
    auto model = Derived();
    model.SortFieldsByIndex();

    // TODO: verify that field indices are unique, contiguous, and start at 1
    // TODO: verify that the primary key is the first field
    // TODO: verify that the primary key is not nullable

    sql << "CREATE TABLE IF NOT EXISTS " << model.m_tableName << " (\n";

    sql << "    " << model.m_primaryKeyName << " " << traits.PrimaryKeyAutoIncrement << ",\n";

    for (auto const* field: model.m_fields)
    {
        sql << "    " << field->Name() << " " << SqlColumnTypeName(field->Type());
        if (field->IsNullable())
            sql << " NULL";
        else
            sql << " NOT NULL";
        if (field != model.m_fields.back())
            sql << ",";
        sql << "\n";
    }

    sql << ");\n";

    return std::move(*sql);
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::CreateTable() noexcept
{
    auto stmt = SqlStatement {};
    return stmt.ExecuteDirect(CreateTableString(stmt.Connection().ServerType()));
}

template <typename Derived>
SqlResult<SqlModelId> SqlModel<Derived>::Create()
{
    auto const requiredFieldCount =
        std::ranges::count_if(m_fields, [](auto const* field) { return field->IsRequired(); });

    auto stmt = SqlStatement {};

    auto const modifiedFields = GetModifiedFields();

    detail::StringBuilder sqlColumnsString;
    detail::StringBuilder sqlValuesString;
    for (auto const* field: modifiedFields)
    {
        if (!field->IsModified())
        {
            // if (field->IsNull() && field->IsRequired())
            //{
            //     SqlLogger::GetLogger().OnWarning( // TODO
            //         std::format("Model required field not given: {}.{}", m_tableName, field->Name()));
            //     return std::unexpected { SqlError::FAILURE }; // TODO: return
            //     SqlError::MODEL_REQUIRED_FIELD_NOT_GIVEN;
            // }
            continue;
        }

        if (!sqlColumnsString.empty())
        {
            sqlColumnsString << ", ";
            sqlValuesString << ", ";
        }

        sqlColumnsString << field->Name(); // TODO: quote column name
        sqlValuesString << "?";
    }

    auto const sqlInsertStmtString =
        std::format("INSERT INTO {} ({}) VALUES ({})", m_tableName, *sqlColumnsString, *sqlValuesString);

    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlInsertStmtString, modifiedFields);

    if (auto result = stmt.Prepare(sqlInsertStmtString); !result)
        return std::unexpected { result.error() };

    for (auto const&& [parameterIndex, field]: modifiedFields | std::views::enumerate)
        if (auto result = field->BindInputParameter(parameterIndex + 1, stmt); !result)
            return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    for (auto* field: m_fields)
        field->SetModified(false);

    // Update the model's ID with the last insert ID
    if (auto const result = stmt.LastInsertId(); result)
        m_id.value = *result;

    return {};
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Load(SqlModelId id)
{
    detail::StringBuilder sqlColumnsString;
    sqlColumnsString << m_primaryKeyName;
    for (SqlModelFieldBase const* field: m_fields)
        sqlColumnsString << ", " << field->Name();

    SqlStatement stmt;

    auto const sqlQueryString = std::format(
        "SELECT {} FROM {} WHERE {} = {} LIMIT 1", *sqlColumnsString, m_tableName, m_primaryKeyName, id.value);

    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, m_fields);

    if (auto result = stmt.Prepare(sqlQueryString); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.BindInputParameter(1, id); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.BindOutputColumn(1, &m_id.value); !result)
        return std::unexpected { result.error() };

    for (SqlModelFieldBase* field: m_fields)
        if (auto result = field->BindOutputColumn(stmt); !result)
            return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.FetchRow(); !result)
        return std::unexpected { result.error() };

    return {};
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Update()
{
    auto sqlColumnsString = detail::StringBuilder {};
    auto modifiedFields = GetModifiedFields();

    for (SqlModelFieldBase* field: modifiedFields)
    {
        if (!field->IsModified())
            continue;

        if (!sqlColumnsString.empty())
            sqlColumnsString << ", ";

        sqlColumnsString << field->Name() << " = ?";
    }

    auto stmt = SqlStatement {};

    auto const sqlQueryString =
        std::format("UPDATE {} SET {} WHERE {} = {}", m_tableName, *sqlColumnsString, m_primaryKeyName, m_id.value);

    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, modifiedFields);

    if (auto result = stmt.Prepare(sqlQueryString); !result)
        return result;

    for (auto const&& [index, field]: modifiedFields | std::views::enumerate)
        if (auto result = field->BindInputParameter(index + 1, stmt); !result)
            return result;

    if (auto result = stmt.Execute(); !result)
        return result;

    for (auto* field: modifiedFields)
        field->SetModified(false);

    return {};
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Save()
{
    if (m_id.value != 0)
        return Update();

    if (auto result = Create(); !result)
        return std::unexpected { result.error() };

    return {};
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Destroy()
{
    auto const sqlQueryString = std::format("DELETE FROM {} WHERE {} = {}", m_tableName, m_primaryKeyName, m_id.value);
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    return SqlStatement {}.ExecuteDirect(sqlQueryString);
}

#pragma endregion

} // namespace Model
