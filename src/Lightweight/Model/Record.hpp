// SPDX-License-Identifier: MIT
#pragma once

#include "../SqlComposedQuery.hpp"
#include "AbstractRecord.hpp"
#include "Detail.hpp"
#include "Field.hpp"
#include "Logger.hpp"

#include <array>
#include <optional>
#include <ranges>
#include <string_view>
#include <vector>

namespace Model
{

enum class SqlWhereOperator : uint8_t
{
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_OR_EQUAL,
    GREATER_THAN,
    GREATER_OR_EQUAL
};

constexpr std::string_view sqlOperatorString(SqlWhereOperator value) noexcept
{
    using namespace std::string_view_literals;

    auto constexpr mappings = std::array {
        "="sv, "!="sv, "<"sv, "<="sv, ">"sv, ">="sv,
    };

    std::string_view result;

    if (std::to_underlying(value) < mappings.size())
        result = mappings[std::to_underlying(value)];

    return result;
}

// API to load records with more complex constraints.
//
// @see Record
// @see Record::Join()
template <typename TargetModel>
class RecordQueryBuilder
{
  private:
    explicit RecordQueryBuilder(SqlSelectQueryBuilder queryBuilder):
        m_queryBuilder { std::move(queryBuilder) }
    {
    }

  public:
    explicit RecordQueryBuilder():
        m_queryBuilder {
            SqlQueryBuilder(SqlConnection().QueryFormatter()).FromTable(std::string(TargetModel().TableName())).Select()
        }
    {
    }

    template <typename JoinModel, StringLiteral foreignKeyColumn>
    [[nodiscard]] RecordQueryBuilder<TargetModel> Join() &&
    {
        auto const joinModel = JoinModel();
        (void) m_queryBuilder.InnerJoin(joinModel.TableName(), joinModel.PrimaryKeyName(), foreignKeyColumn.value);
        return RecordQueryBuilder<TargetModel> { std::move(m_queryBuilder) };
    }

    [[nodiscard]] RecordQueryBuilder<TargetModel> Join(std::string_view joinTableName,
                                                       std::string_view joinTablePrimaryKey,
                                                       SqlQualifiedTableColumnName onComparisonColumn) &&
    {
        (void) m_queryBuilder.InnerJoin(joinTableName, joinTablePrimaryKey, onComparisonColumn);
        return RecordQueryBuilder<TargetModel> { std::move(m_queryBuilder) };
    }

    [[nodiscard]] RecordQueryBuilder<TargetModel> Join(std::string_view joinTableName,
                                                       std::string_view joinTablePrimaryKey,
                                                       std::string_view onComparisonColumn) &&
    {
        (void) m_queryBuilder.InnerJoin(joinTableName, joinTablePrimaryKey, onComparisonColumn);
        return RecordQueryBuilder<TargetModel> { std::move(m_queryBuilder) };
    }

    template <typename ColumnName, typename T>
    [[nodiscard]] RecordQueryBuilder Where(ColumnName const& columnName,
                                           SqlWhereOperator whereOperator,
                                           T const& value) &&
    {
        (void) m_queryBuilder.Where(columnName, sqlOperatorString(whereOperator), value);
        return RecordQueryBuilder<TargetModel> { std::move(m_queryBuilder) };
    }

    template <typename ColumnName, typename T>
    [[nodiscard]] RecordQueryBuilder Where(ColumnName const& columnName, T const& value) &&
    {
        (void) m_queryBuilder.Where(columnName, value);
        return *this;
    }

    [[nodiscard]] RecordQueryBuilder OrderBy(std::string_view columnName,
                                             SqlResultOrdering ordering = SqlResultOrdering::ASCENDING) &&
    {
        (void) m_queryBuilder.OrderBy(columnName, ordering);
        return *this;
    }

    [[nodiscard]] std::size_t Count()
    {
        auto stmt = SqlStatement();
        auto const sqlQueryString = m_queryBuilder.Count().ToSql();
        auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
        return stmt.ExecuteDirectScalar<size_t>(sqlQueryString).value();
    }

    [[nodiscard]] std::optional<TargetModel> First(size_t count = 1)
    {
        TargetModel targetRecord;

        auto stmt = SqlStatement {};

        auto const sqlQueryString =
            m_queryBuilder.Fields(targetRecord.AllFieldNames(), targetRecord.TableName()).First(count).ToSql();

        auto const _ = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

        stmt.Prepare(sqlQueryString);
        stmt.Execute();

        stmt.BindOutputColumn(1, &targetRecord.MutableId().value);
        for (AbstractField* field: targetRecord.AllFields())
            field->BindOutputColumn(stmt);

        if (!stmt.FetchRow())
            return std::nullopt;

        return { std::move(targetRecord) };
    }

    [[nodiscard]] std::vector<TargetModel> Range(std::size_t offset, std::size_t limit)
    {
        auto const targetRecord = TargetModel();
        auto const sqlQueryString =
            m_queryBuilder.Field(targetRecord.AllFieldNames(), targetRecord.TableName()).Range(offset, limit).ToSql();
        return TargetModel::Query(sqlQueryString).value_or(std::vector<TargetModel> {});
    }

    template <typename Callback>
    void Each(Callback&& callback)
    {
        auto const targetRecord = TargetModel();
        auto const sqlQueryString =
            m_queryBuilder.Fields(targetRecord.AllFieldNames(), targetRecord.TableName()).All().ToSql();
        TargetModel::Each(std::forward<Callback>(callback), sqlQueryString);
    }

    [[nodiscard]] std::vector<TargetModel> All()
    {
        auto const targetRecord = TargetModel();
        auto const sqlQueryString =
            m_queryBuilder.Fields(targetRecord.AllFieldNames(), targetRecord.TableName()).All().ToSql();
        return TargetModel::Query(sqlQueryString);
    }

  private:
    SqlSelectQueryBuilder m_queryBuilder;
};

template <typename Derived>
struct Record: AbstractRecord
{
  public:
    static constexpr auto Nullable = Model::SqlNullable;
    static constexpr auto NotNullable = Model::SqlNotNullable;

    Record() = delete;
    Record(Record const&) = default;
    Record& operator=(Record const&) = delete;
    Record& operator=(Record&&) noexcept = default;
    ~Record() override = default;

    Record(Record&& other) noexcept:
        AbstractRecord(std::move(other))
    {
    }

    // Creates (or recreates a copy of) the model in the database.
    RecordId Create();

    // Reads the model from the database by given model ID.
    bool Load(RecordId id);

    // Re-reads the model from the database.
    void Reload();

    // Reads the model from the database by given column name and value.
    template <typename T>
    bool Load(std::string_view const& columnName, T const& value);

    // Updates the model in the database.
    void Update();

    // Creates or updates the model in the database, depending on whether it already exists.
    void Save();

    // Deletes the model from the database.
    void Destroy();

    // Updates all models with the given changes in the modelChanges model.
    static void UpdateAll(Derived const& modelChanges) noexcept;

    // Retrieves the first model from the database (ordered by ID ASC).
    static std::optional<Derived> First(size_t count = 1);

    // Retrieves the last model from the database (ordered by ID ASC).
    static std::optional<Derived> Last();

    // Retrieves the model with the given ID from the database.
    static std::optional<Derived> Find(RecordId id);

    template <typename ColumnName, typename T>
    static std::optional<Derived> FindBy(ColumnName const& columnName, T const& value);

    // Retrieves all models of this kind from the database.
    static std::vector<Derived> All() noexcept;

    // Retrieves the number of models of this kind from the database.
    static size_t Count() noexcept;

    static RecordQueryBuilder<Derived> Build();

    // Joins the model with this record's model and returns a proxy for further joins and actions on this join.
    template <typename JoinModel, StringLiteral foreignKeyColumn>
    static RecordQueryBuilder<Derived> Join();

    static RecordQueryBuilder<Derived> Join(std::string_view joinTable,
                                            std::string_view joinColumnName,
                                            SqlQualifiedTableColumnName onComparisonColumn);

    template <typename Value>
    static RecordQueryBuilder<Derived> Where(std::string_view columnName,
                                             SqlWhereOperator whereOperator,
                                             Value const& value);

    template <typename Value>
    static RecordQueryBuilder<Derived> Where(std::string_view columnName, Value const& value);

    // Invokes a callback for each model that matches the given query string.
    template <typename Callback, typename... InputParameters>
    static void Each(Callback&& callback, std::string_view sqlQueryString, InputParameters&&... inputParameters);

    template <typename... InputParameters>
    static std::vector<Derived> Query(std::string_view sqlQueryString, InputParameters&&... inputParameters);

    // Returns the SQL string to create the table for this model.
    static std::string CreateTableString(SqlServerType serverType);

    // Creates the table for this model from the database.
    static void CreateTable();

    // Drops the table for this model from the database.
    static void DropTable();

  protected:
    explicit Record(std::string_view tableName, std::string_view primaryKey = "id");
};

// {{{ Record<> implementation

template <typename Derived>
Record<Derived>::Record(std::string_view tableName, std::string_view primaryKey):
    AbstractRecord { tableName, primaryKey, RecordId {} }
{
}

template <typename Derived>
size_t Record<Derived>::Count() noexcept
{
    SqlStatement stmt;
    return stmt.ExecuteDirectScalar<size_t>(std::format("SELECT COUNT(*) FROM {}", Derived().TableName())).value();
}

template <typename Derived>
std::optional<Derived> Record<Derived>::Find(RecordId id)
{
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for Find() to return the model.");
    Derived model;
    if (!model.Load(id))
        return std::nullopt;
    return { std::move(model) };
}

template <typename Derived>
template <typename ColumnName, typename T>
std::optional<Derived> Record<Derived>::FindBy(ColumnName const& columnName, T const& value)
{
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for Find() to return the model.");
    Derived model;
    if (!model.Load(columnName, value))
        return std::nullopt;
    return { std::move(model) };
}

template <typename Derived>
std::vector<Derived> Record<Derived>::All() noexcept
{
    // Require that the model is copy constructible. Simply add a default move constructor to the model if it is not.
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for All() to copy elements into the result.");

    std::vector<Derived> allModels;

    Derived const modelSchema;

    detail::StringBuilder sqlColumnsString;
    sqlColumnsString << modelSchema.PrimaryKeyName();
    for (AbstractField const* field: modelSchema.AllFields())
        sqlColumnsString << ", " << field->Name();

    SqlStatement stmt;

    auto const sqlQueryString = std::format("SELECT {} FROM {}", *sqlColumnsString, modelSchema.TableName());

    auto scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    stmt.Prepare(sqlQueryString);
    stmt.Execute();

    while (true)
    {
        Derived record;

        stmt.BindOutputColumn(1, &record.m_data->id.value);
        for (AbstractField* field: record.AllFields())
            field->BindOutputColumn(stmt);

        if (!stmt.FetchRow())
            break;

        scopedModelSqlLogger += record;

        allModels.emplace_back(std::move(record));
    }

    return allModels;
}

template <typename Derived>
std::string Record<Derived>::CreateTableString(SqlServerType serverType)
{
    SqlTraits const& traits = GetSqlTraits(serverType); // TODO: take server type from connection
    detail::StringBuilder sql;
    auto model = Derived();
    model.SortFieldsByIndex();

    // TODO: verify that field indices are unique, contiguous, and start at 1
    // TODO: verify that the primary key is the first field
    // TODO: verify that the primary key is not nullable

    sql << "CREATE TABLE " << model.TableName() << " (\n";

    sql << "    " << model.PrimaryKeyName() << " " << traits.PrimaryKeyAutoIncrement << ",\n";

    std::vector<std::string> sqlConstraints;

    for (auto const* field: model.AllFields())
    {
        sql << "    " << field->Name() << " " << traits.ColumnTypeName(field->Type());

        if (field->IsNullable())
            sql << " NULL";
        else
            sql << " NOT NULL";

        if (auto constraint = field->SqlConstraintSpecifier(); !constraint.empty())
            sqlConstraints.emplace_back(std::move(constraint));

        if (field != model.AllFields().back() || !sqlConstraints.empty())
            sql << ",";
        sql << "\n";
    }

    for (auto const& constraint: sqlConstraints)
    {
        sql << "    " << constraint;
        if (&constraint != &sqlConstraints.back())
            sql << ",";
        sql << "\n";
    }

    sql << ");\n";

    return *sql;
}

template <typename Derived>
void Record<Derived>::CreateTable()
{
    auto stmt = SqlStatement {};
    auto const sqlQueryString = CreateTableString(stmt.Connection().ServerType());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    stmt.ExecuteDirect(sqlQueryString);
}

template <typename Derived>
void Record<Derived>::DropTable()
{
    auto const sqlQueryString = std::format("DROP TABLE \"{}\"", Derived().TableName());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    SqlStatement().ExecuteDirect(sqlQueryString);
}

template <typename Derived>
RecordId Record<Derived>::Create()
{
    auto stmt = SqlStatement {};

    auto const modifiedFields = GetModifiedFields();

    detail::StringBuilder sqlColumnsString;
    detail::StringBuilder sqlValuesString;
    for (auto const* field: modifiedFields)
    {
        if (!field->IsModified())
        {
            // if (field->IsNull() && field->IsRequired())
            // {
            //     SqlLogger::GetLogger().OnWarning( // TODO
            //         std::format("Model required field not given: {}.{}", TableName(), field->Name()));
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

        sqlColumnsString << field->Name();
        sqlValuesString << "?";
    }

    auto const sqlInsertStmtString =
        std::format("INSERT INTO {} ({}) VALUES ({})", TableName(), *sqlColumnsString, *sqlValuesString);

    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlInsertStmtString, modifiedFields);

    stmt.Prepare(sqlInsertStmtString);

    for (auto const&& [parameterIndex, field]: modifiedFields | std::views::enumerate)
        field->BindInputParameter(parameterIndex + 1, stmt);

    stmt.Execute();

    for (auto* field: AllFields())
        field->SetModified(false);

    // Update the model's ID with the last insert ID
    m_data->id = RecordId { .value = stmt.LastInsertId() };
    return m_data->id;
}

template <typename Derived>
bool Record<Derived>::Load(RecordId id)
{
    return Load(PrimaryKeyName(), id.value);
}

template <typename Derived>
void Record<Derived>::Reload()
{
    Load(PrimaryKeyName(), Id());
}

template <typename Derived>
template <typename T>
bool Record<Derived>::Load(std::string_view const& columnName, T const& value)
{
    SqlStatement stmt;

    auto const sqlQueryString = SqlQueryBuilder(stmt.Connection().QueryFormatter())
                                    .FromTable(std::string(TableName()))
                                    .Select()
                                    .Fields(AllFieldNames())
                                    .Where(columnName, SqlQueryWildcard())
                                    .First()
                                    .ToSql();

    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, AllFields());

    stmt.Prepare(sqlQueryString);
    stmt.BindInputParameter(1, value);
    stmt.BindOutputColumn(1, &m_data->id.value);
    for (AbstractField* field: AllFields())
        field->BindOutputColumn(stmt);
    stmt.Execute();
    return stmt.FetchRow();
}

template <typename Derived>
void Record<Derived>::Update()
{
    auto sqlColumnsString = detail::StringBuilder {};
    auto modifiedFields = GetModifiedFields();

    for (AbstractField* field: modifiedFields)
    {
        if (!field->IsModified())
            continue;

        if (!sqlColumnsString.empty())
            sqlColumnsString << ", ";

        sqlColumnsString << field->Name() << " = ?";
    }

    auto stmt = SqlStatement {};

    auto const sqlQueryString =
        std::format("UPDATE {} SET {} WHERE {} = {}", TableName(), *sqlColumnsString, PrimaryKeyName(), Id());

    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, modifiedFields);

    stmt.Prepare(sqlQueryString);

    for (auto const&& [index, field]: modifiedFields | std::views::enumerate)
        field->BindInputParameter(index + 1, stmt);

    stmt.Execute();

    for (auto* field: modifiedFields)
        field->SetModified(false);
}

template <typename Derived>
void Record<Derived>::Save()
{
    if (Id().value != 0)
        return Update();

    Create();
}

template <typename Derived>
void Record<Derived>::Destroy()
{
    auto const sqlQueryString = std::format("DELETE FROM {} WHERE {} = {}", TableName(), PrimaryKeyName(), *Id());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    auto stmt = SqlStatement {};
    auto const& sqlTraits = stmt.Connection().Traits();
    stmt.ExecuteDirect(sqlTraits.EnforceForeignKeyConstraint);
    stmt.ExecuteDirect(sqlQueryString);
}

template <typename Derived>
RecordQueryBuilder<Derived> Record<Derived>::Build()
{
    return RecordQueryBuilder<Derived>();
}

template <typename Derived>
template <typename JoinModel, StringLiteral foreignKeyColumn>
RecordQueryBuilder<Derived> Record<Derived>::Join()
{
    return RecordQueryBuilder<Derived>().template Join<JoinModel, foreignKeyColumn>();
}

template <typename Derived>
RecordQueryBuilder<Derived> Record<Derived>::Join(std::string_view joinTable,
                                                  std::string_view joinColumnName,
                                                  SqlQualifiedTableColumnName onComparisonColumn)
{
    return RecordQueryBuilder<Derived>().Join(joinTable, joinColumnName, onComparisonColumn);
}

template <typename Derived>
template <typename Value>
RecordQueryBuilder<Derived> Record<Derived>::Where(std::string_view columnName, Value const& value)
{
    return Where(columnName, SqlWhereOperator::EQUAL, value);
}

template <typename Derived>
template <typename Value>
RecordQueryBuilder<Derived> Record<Derived>::Where(std::string_view columnName,
                                                   SqlWhereOperator whereOperator,
                                                   Value const& value)
{
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for Where() to return the models.");

#if 1
    return RecordQueryBuilder<Derived>().Where(columnName, whereOperator, value);
#else
    std::vector<Derived> allModels;

    Derived modelSchema;

    detail::StringBuilder sqlColumnsString;
    sqlColumnsString << modelSchema.PrimaryKeyName();
    for (AbstractField const* field: modelSchema.AllFields())
        sqlColumnsString << ", " << field->Name();

    auto const sqlQueryString = std::format("SELECT {} FROM {} WHERE \"{}\" {} ?",
                                            *sqlColumnsString,
                                            modelSchema.TableName(),
                                            columnName,
                                            sqlOperatorString(whereOperator));
    return Query(sqlQueryString, value);
#endif
}

template <typename Derived>
template <typename... InputParameters>
std::vector<Derived> Record<Derived>::Query(std::string_view sqlQueryString, InputParameters&&... inputParameters)
{
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for Where() to return the models.");

    std::vector<Derived> output;
    Each([&output](Derived& model) { output.push_back(std::move(model)); },
         sqlQueryString,
         std::forward<InputParameters>(inputParameters)...);
    return { std::move(output) };
}

template <typename Derived>
template <typename Callback, typename... InputParameters>
void Record<Derived>::Each(Callback&& callback, std::string_view sqlQueryString, InputParameters&&... inputParameters)
{
    SqlStatement stmt;

    auto scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    stmt.Prepare(sqlQueryString);

    SQLSMALLINT inputParameterPosition = 0;
    (stmt.BindInputParameter(++inputParameterPosition, std::forward<InputParameters>(inputParameters)), ...);

    stmt.Execute();

    while (true)
    {
        Derived record;

        stmt.BindOutputColumn(1, &record.m_data->id.value);
        for (AbstractField* field: record.AllFields())
            field->BindOutputColumn(stmt);

        if (!stmt.FetchRow())
            break;

        scopedModelSqlLogger += record;

        callback(record);
    }
}

// }}}

} // namespace Model
