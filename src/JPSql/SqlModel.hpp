#pragma once

// Skatching the surface of a modern C++23 Object Relational Mapping (ORM) layer
//
// We usually know exactly the schema of the database we are working with,
// but we need to be able to address columns with an unknown name but known index,
// and we need to be able to deal with tables of unknown column count.

#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"
#include "SqlStatement.hpp"

#include <algorithm>
#include <compare>
#include <ranges>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

template <size_t N>
struct SqlStringLiteral
{
    constexpr SqlStringLiteral(const char (&str)[N]) noexcept
    {
        std::copy_n(str, N, value);
    }

    char value[N];
};

struct SqlColumnIndex
{
    size_t value;
};

namespace detail
{
struct StringBuilder
{
    std::string output;

    std::string operator*() const& noexcept
    {
        return std::move(output);
    }
    std::string operator*() && noexcept
    {
        return std::move(output);
    }

    bool empty() const noexcept
    {
        return output.empty();
    }

    template <typename T>
    StringBuilder& operator<<(T&& value)
    {
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>
                      || std::is_same_v<T, char const*>)
            output += value;
        else
            output += std::format("{}", std::forward<T>(value));
        return *this;
    }
};
} // namespace detail

// {{{ SqlModelId
struct SqlModelId
{
    size_t value;

    constexpr std::strong_ordering operator<=>(SqlModelId const& other) const noexcept = default;
};

template <>
struct SqlDataBinder<SqlModelId>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLSMALLINT column, SqlModelId const& value)
    {
        return SqlDataBinder<decltype(value.value)>::InputParameter(stmt, column, value.value);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLSMALLINT column, SqlModelId* result, SQLLEN* indicator, SqlDataBinderCallback& cb)
    {
        return SqlDataBinder<decltype(result->value)>::OutputColumn(stmt, column, &result->value, indicator, cb);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLSMALLINT column, SqlModelId* result, SQLLEN* indicator)
    {
        return SqlDataBinder<decltype(result->value)>::GetColumn(stmt, column, &result->value, indicator);
    }
};
// }}}

// {{{ schema
enum class SqlWhereOperator : uint8_t
{
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    GREATER_THAN,
    LESS_OR_EQUAL,
    GREATER_OR_EQUAL
};

enum class SqlColumnType : uint8_t
{
    UNKNOWN,
    STRING,
    BOOLEAN,
    INTEGER,
    REAL,
    BLOB,
    DATE,
    TIME,
    TIMESTAMP,
};

constexpr std::string_view SqlColumnTypeName(SqlColumnType value) noexcept
{
    switch (value)
    {
        case SqlColumnType::STRING:
            return "TEXT";
        case SqlColumnType::BOOLEAN:
            return "BOOLEAN";
        case SqlColumnType::INTEGER:
            return "INTEGER";
        case SqlColumnType::REAL:
            return "REAL";
        case SqlColumnType::BLOB:
            return "BLOB";
        case SqlColumnType::DATE:
            return "DATE";
        case SqlColumnType::TIME:
            return "TIME";
        case SqlColumnType::TIMESTAMP:
            return "TIMESTAMP";
    }
    return "UNKNOWN";
}

namespace detail
{
template <typename>
struct SqlColumnTypeOf;

// clang-format off
template <> struct SqlColumnTypeOf<std::string> { static constexpr SqlColumnType value = SqlColumnType::STRING; };
template <> struct SqlColumnTypeOf<bool> { static constexpr SqlColumnType value = SqlColumnType::BOOLEAN; };
template <> struct SqlColumnTypeOf<int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<float> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct SqlColumnTypeOf<double> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct SqlColumnTypeOf<SqlDate> { static constexpr SqlColumnType value = SqlColumnType::DATE; };
template <> struct SqlColumnTypeOf<SqlTime> { static constexpr SqlColumnType value = SqlColumnType::TIME; };
template <> struct SqlColumnTypeOf<SqlTimestamp> { static constexpr SqlColumnType value = SqlColumnType::TIMESTAMP; };
template <> struct SqlColumnTypeOf<SqlModelId> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
// clang-format on
} // namespace detail

template <typename T>
constexpr SqlColumnType SqlColumnTypeOf = detail::SqlColumnTypeOf<T>::value;

struct SqlColumnSchema
{
    // std::string name;
    SqlColumnType type = SqlColumnType::UNKNOWN;
    bool isNullable : 1 = false;
    bool isPrimaryKey : 1 = false;
    bool isAutoIncrement : 1 = false;
    bool isUnique : 1 = false;
    bool isInded : 1 = false;
    unsigned columnSize = 0;
};

// Loads and represents a table schema from the database
class SqlTableSchema
{
  public:
    using ColumnList = std::vector<SqlColumnSchema>;
    SqlTableSchema(std::string tableName, ColumnList columns);

    std::string const& TableName() const noexcept;
    SqlColumnSchema const& Columns() const noexcept;

    SqlResult<SqlTableSchema> Load(SqlConnection& connection, std::string tableName);

  private:
    std::string m_tableName;
    std::vector<SqlColumnSchema> m_columns;
};
// }}}

// Forward declarations
struct SqlModelBase;

template <typename Derived>
struct SqlModel;

template <typename... Models>
SqlResult<void> CreateSqlTables();

enum class SqlFieldValueRequirement : uint8_t
{
    NULLABLE,
    NOT_NULL,
};

constexpr inline SqlFieldValueRequirement SqlNullable = SqlFieldValueRequirement::NULLABLE;
constexpr inline SqlFieldValueRequirement SqlNotNullable = SqlFieldValueRequirement::NULLABLE;

// Base class for all fields in a model.
class SqlModelFieldBase
{
  public:
    SqlModelFieldBase(SqlModelBase& model,
                      SQLSMALLINT index,
                      std::string_view name,
                      SqlColumnType type,
                      SqlFieldValueRequirement requirement):
        m_model { &model },
        m_index { index },
        m_name { name },
        m_type { type },
        m_requirement { requirement }
    {
    }

    virtual ~SqlModelFieldBase() = default;

    virtual std::string InspectValue() const = 0;
    virtual SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const = 0;
    virtual SqlResult<void> BindOutputColumn(SqlStatement& stmt) = 0;

    // clang-format off
    SqlModelBase& Model() noexcept { return *m_model; }
    SqlModelBase const& Model() const noexcept { return *m_model; }
    bool IsModified() const noexcept { return m_modified; }
    void SetModified(bool value) noexcept { m_modified = value; }
    SQLSMALLINT Index() const noexcept { return m_index; }
    std::string_view Name() const noexcept { return m_name; }
    SqlColumnType Type() const noexcept { return m_type; }
    bool IsNullable() const noexcept { return m_requirement == SqlFieldValueRequirement::NULLABLE; }
    bool IsRequired() const noexcept { return m_requirement == SqlFieldValueRequirement::NOT_NULL; }
    // clang-format on

  private:
    SqlModelBase* m_model;
    SQLSMALLINT m_index;
    std::string_view m_name;
    SqlColumnType m_type;
    SqlFieldValueRequirement m_requirement;
    bool m_modified = false;
};

class SqlModelRelation
{
  public:
    virtual ~SqlModelRelation() = default;
};

// Base class for every SqlModel<T>.
class SqlModelBase
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

    void SortFieldsByIndex() noexcept
    {
        std::sort(m_fields.begin(), m_fields.end(), [](auto a, auto b) { return a->Index() < b->Index(); });
    }

  protected:
    std::string_view m_tableName;      // Should not be modified, but we want to allow move semantics
    std::string_view m_primaryKeyName; // Should not be modified, but we want to allow move semantics
    SqlModelId m_id { std::numeric_limits<size_t>::max() };

    std::vector<SqlModelFieldBase*> m_fields;
    std::vector<SqlModelRelation*> m_relations;
};

// Represents a single column in a table.
//
// The column name, index, and type are known at compile time.
// If either name or index are not known at compile time, leave them at their default values,
// but at least one of them msut be known.
template <typename T,
          SQLSMALLINT TheTableColumnIndex = 0,
          SqlStringLiteral TheColumnName = "",
          SqlFieldValueRequirement TheRequirement = SqlFieldValueRequirement::NOT_NULL>
class SqlModelField final: public SqlModelFieldBase
{
  public:
    explicit SqlModelField(SqlModelBase& registry):
        SqlModelFieldBase {
            registry, TheTableColumnIndex, TheColumnName.value, SqlColumnTypeOf<T>, TheRequirement,
        }
    {
        registry.RegisterField(*this);
    }

    explicit SqlModelField(SqlModelBase& registry, SqlModelField&& field):
        SqlModelFieldBase {
            registry, TheTableColumnIndex, TheColumnName.value, SqlColumnTypeOf<T>, TheRequirement,
        },
        m_value { field.Value() }
    {
        registry.RegisterField(*this);
    }

    SqlModelField(SqlModelField&& other):
        SqlModelFieldBase { std::move(other) },
        m_value { std::move(other.m_value) }
    {
        Model().UnregisterField(other);
        Model().RegisterField(*this);
    }

    SqlModelField& operator=(SqlModelField&& other)
    {
        if (this != &other)
        {
            SqlModelFieldBase::operator=(std::move(other));
            m_value = std::move(other.m_value);
        }
        return *this;
    }

    SqlModelField& operator=(SqlModelField const& other)
    {
        if (this != &other)
        {
            SqlModelFieldBase::operator=(std::move(other));
            m_value = other.m_value;
        }
        return *this;
    }

    SqlModelField(SqlModelField const& other):
        SqlModelFieldBase { other },
        m_value { other.m_value }
    {
        Model().UnregisterField(other); // ReregisterField(other, *this);
        Model().RegisterField(*this);
    }

    // clang-format off

    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    auto operator<=>(SqlModelField<U, I, N, R> const& other) const noexcept { return m_value <=> other.m_value; }

    // We also define the equality and inequality operators explicitly, because <=> from above does not seem to work in MSVC VS 2022.
    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    auto operator==(SqlModelField<U, I, N, R> const& other) const noexcept { return m_value == other.m_value; }

    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    auto operator!=(SqlModelField<U, I, N, R> const& other) const noexcept { return m_value != other.m_value; }

    T const& Value() const noexcept { return m_value; }
    void SetData(T&& value) { SetModified(true); m_value = std::move(value); }
    void SetNull() { SetModified(true); m_value = T {}; }

    SqlModelField& operator=(T&& value) noexcept;

    T& operator*() noexcept;
    T const& operator*() const noexcept;

    // clang-format on

    std::string InspectValue() const override;
    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;

  private:
    T m_value {};
};

// Represents a column in a table that is a foreign key to another table.
template <typename ModelType,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement = SqlFieldValueRequirement::NOT_NULL>
class SqlModelBelongsTo final: public SqlModelFieldBase
{
  public:
    explicit SqlModelBelongsTo(SqlModelBase& registry):
        SqlModelFieldBase {
            registry, TheColumnIndex, TheForeignKeyName.value, SqlColumnTypeOf<SqlModelId>, TheRequirement,
        }
    {
        registry.RegisterField(*this);
    }

    explicit SqlModelBelongsTo(SqlModelBase& registry, SqlModelBelongsTo&& other):
        SqlModelFieldBase { std::move(other) },
        m_value { other.m_value }
    {
        registry.RegisterField(*this);
    }

    SqlModelBelongsTo& operator=(SqlModelId modelId) noexcept;
    SqlModelBelongsTo& operator=(SqlModel<ModelType> const& model) noexcept;

    ModelType* operator->() noexcept; // TODO
    ModelType& operator*() noexcept;  // TODO

    constexpr static inline SQLSMALLINT ColumnIndex { TheColumnIndex };
    constexpr static inline std::string_view ColumnName { TheForeignKeyName.value };

    std::string InspectValue() const override;
    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;

    auto operator<=>(SqlModelBelongsTo const& other) const noexcept { return m_value <=> other.m_value; }

    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    bool operator==(SqlModelBelongsTo<U, I, N, R> const& other) const noexcept { return m_value == other.m_value; }
    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    bool operator!=(SqlModelBelongsTo<U, I, N, R> const& other) const noexcept { return m_value == other.m_value; }

  private:
    SqlModelId m_value {};
};

// Represents an association of another Model with a foreign key to this model.
template <typename Model>
class HasOne final: public SqlModelRelation
{
  public:
    explicit HasOne(SqlModelBase& registry)
    {
        registry.RegisterRelation(*this);
    }

    Model* operator->() noexcept; // TODO
    Model& operator*() noexcept;  // TODO

    void Load() noexcept;           // TODO
    bool IsLoaded() const noexcept; // TODO

  private:
    std::optional<Model> m_model;
};

template <typename Model, SqlStringLiteral ForeignKeyName>
class HasMany: public SqlModelRelation
{
  public:
    size_t Count() const noexcept;

    std::vector<Model>& All() noexcept
    {
        RequireLoaded();
        return m_models;
    }

    explicit HasMany(SqlModelBase& model):
        m_model { &model }
    {
        model.RegisterRelation(*this);
    }

    bool IsLoaded() const noexcept
    {
        return m_loaded;
    }

    void Load();
    void Reload();

    bool IsEmpty() const noexcept
    {
        RequireLoaded();
        return m_models.empty();
    }

    SqlResult<size_t> Size() const noexcept
    {
        if (m_loaded)
            return m_models.size();

        SqlStatement stmt;

        auto const tableName = Model().TableName(); // TODO: cache schema information once
        auto const id = m_model->Id().value;
#if 0 // TODO: enable this instead
        stmt.Prepare(std::format("SELECT COUNT(*) FROM {} WHERE {} = {}", tableName, ForeignKeyName.value, id))
            .and_then([&] { return stmt.Execute(); })
            .and_then([&] { return stmt.FetchRow(); })
            .and_then([&] { return stmt.GetColumn<size_t>(1); });
#else
        if (auto result =
                stmt.Prepare(std::format("SELECT COUNT(*) FROM {} WHERE {} = {}", tableName, ForeignKeyName.value, id));
            !result)
            return std::unexpected { result.error() };

        if (auto result = stmt.Execute(); !result)
            return std::unexpected { result.error() };

        if (auto result = stmt.FetchRow(); !result)
            return std::unexpected { result.error() };

        return stmt.GetColumn<size_t>(1);
#endif
    }

    Model& At(size_t index) noexcept
    {
        RequireLoaded();
        return m_models.at(index);
    }

    Model& operator[](size_t index) noexcept
    {
        RequireLoaded();
        return m_models[index];
    }

  private:
    bool RequireLoaded();

    bool m_loaded = false;
    SqlModelBase* m_model;
    std::vector<Model> m_models;
};

#if 0 // Builder API
// This SQL query builder is used to build the queries for the SqlModel.
namespace SqlQueryBuilder
{

class Builder
{
  public:
    Builder() = default;

    Builder& Select(const std::string& columns) &&;
    Builder& Update(const std::string& columns) &&;
    Builder& Delete() &&;
    Builder& From(const std::string& table) &&;
    Builder& Where(const std::string& column, const SqlVariant& value, SqlWhereOperator whereOperator) &&;
    Builder& OrderBy(const std::string& column, bool ascending = true) &&;
    Builder& Limit(size_t limit) &&;
    Builder& Offset(size_t offset) &&;

    std::string Build() const&&;

    class ColumnsBuilder
    {
    };
    class BuilderStart
    {
        Columns
    };

  private:
    std::string m_query;
};

SqlQueryBuilder::Selecting Select();

} // namespace SqlQueryBuilder

inline auto testBuildQuery()
{
    return SqlQueryBuilder::Builder {}.Select("name").From("persons").Where("id", 1);
}
#endif

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

// TODO: Design a model schema registry, such that an instantiation of a model for
//       regular data use is as efficient as possible.
class SqlModelSchemaRegistry
{
  public:
    template <typename Model>
    void RegisterModel()
    {
        // TODO: m_modelSchemas.emplace_back(SqlModel<Model>(*this));
    }

  private:
    // std::vector<SqlModelSchema> m_modelSchemas;
};

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

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
std::string SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::InspectValue() const
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::stringstream result;
        result << std::quoted(m_value);
        return std::move(result.str());
    }
    else if constexpr (std::is_same_v<T, SqlDate>)
        return std::format("\"{}\"", m_value.value);
    else if constexpr (std::is_same_v<T, SqlTime>)
        return std::format("\"{}\"", m_value.value);
    else if constexpr (std::is_same_v<T, SqlTimestamp>)
        return std::format("\"{}\"", m_value.value);
    else
        return std::format("{}", m_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindInputParameter(
    SQLSMALLINT parameterIndex, SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, m_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindOutputColumn(
    SqlStatement& stmt)
{
    SetModified(false);

    return stmt.BindOutputColumn(TheTableColumnIndex, &m_value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
std::string SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::InspectValue() const
{
    return std::to_string(m_value.value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindInputParameter(
    SQLSMALLINT parameterIndex, SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, m_value.value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindOutputColumn(
    SqlStatement& stmt)
{
    return {};
    (void) stmt; // TODO: return stmt.BindOutputColumn(TheColumnIndex, &m_value.value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>&
SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::operator=(T&& value) noexcept
{
    SetModified(true);
    m_value = std::move(value);
    return *this;
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>&
SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator=(SqlModelId modelId) noexcept
{
    SetModified(true);
    m_value = modelId;
    return *this;
}

template <typename ModelType,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlModelBelongsTo<ModelType, TheColumnIndex, TheForeignKeyName, TheRequirement>& SqlModelBelongsTo<
    ModelType,
    TheColumnIndex,
    TheForeignKeyName,
    TheRequirement>::operator=(SqlModel<ModelType> const& model) noexcept
{
    SetModified(true);
    m_value = model.Id();
    return *this;
}

// ----------------------------------------------------------------------------------------------------------------
// SqlModel

template <typename Derived>
SqlResult<SqlModelId> SqlModel<Derived>::Create()
{
    auto const requiredFieldCount =
        std::ranges::count_if(m_fields, [](auto const* field) { return field->IsRequired(); });

    auto stmt = SqlStatement {};

    detail::StringBuilder sqlColumnsString;
    detail::StringBuilder sqlValuesString;
    for (auto const* field: m_fields)
    {
        if (!field->IsModified())
        {
            // if (field->IsNull() && field->IsRequired())
            //{
            //     SqlLogger::GetLogger().OnWarning(
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

    std::print("Creating model with SQL: {}\n", sqlInsertStmtString);

    if (auto result = stmt.Prepare(sqlInsertStmtString); !result)
        return std::unexpected { result.error() };

    SQLSMALLINT parameterIndex = 1;
    for (auto const* field: m_fields)
        if (field->IsModified())
            if (auto result = field->BindInputParameter(parameterIndex++, stmt); !result)
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

    if (auto result = stmt.Prepare(
            std::format("SELECT {} FROM {} WHERE {} = ? LIMIT 1", *sqlColumnsString, m_tableName, m_primaryKeyName));
        !result)
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
    auto stmt = SqlStatement {};
    auto columns = std::string {}; // TODO
#if 0                              // TODO
    if (auto result = stmt.Prepare("UPDATE ? SET {} WHERE {} = ?", m_tableName, m_primaryKeyName); !result)
        // TODO: number of "?" must match number of parameters
        return result;

    for (auto const* field: m_fields)
        if (field->IsModified())
            if (auto result = field->BindInputParameter(stmt); !result)
                return result;

    if (auto result = stmt.Execute(); !result)
        return result;

    for (auto& field: m_fields)
        field.ResetModified();
#endif
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
    auto stmt = SqlStatement {};
    return stmt.ExecuteDirect(std::format("DELETE FROM {} WHERE {} = {}", m_tableName, m_primaryKeyName, m_id.Value()));
}

// ----------------------------------------------------------------------------------------------------------------
// HasMany<T>

template <typename Model, SqlStringLiteral ForeignKeyName>
size_t HasMany<Model, ForeignKeyName>::Count() const noexcept
{
    if (!m_models.empty())
        return m_models.size();

    auto stmt = SqlStatement {};
    auto const conceptModel = Model();
    auto result = stmt.Prepare(
        std::format("SELECT COUNT(*) FROM {} WHERE {} = ?", conceptModel.TableName(), conceptModel.PrimaryKeyName()));
    if (!stmt.Execute())
        return 0;
    if (!stmt.FetchRow())
        return 0;
    return stmt.GetColumn<size_t>(1);
}

// ----------------------------------------------------------------------------------------------------------------
// free functions

template <typename... Models>
std::string CreateSqlTablesString(SqlServerType serverType)
{
    std::string result;
    result = ((Models::CreateTableString(serverType) + "\n") + ...);
    return result;
}

template <typename... Models>
SqlResult<void> CreateSqlTables()
{
    SqlResult<void> result;
    ((result = Models::CreateTable()) && ...);
    return result;
}
