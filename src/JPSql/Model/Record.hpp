// SPDX-License-Identifier: MIT
#pragma once

#include "../SqlComposedQuery.hpp"
#include "AbstractRecord.hpp"
#include "Detail.hpp"
#include "Field.hpp"
#include "Logger.hpp"

#include <array>
#include <limits>
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
    explicit RecordQueryBuilder(SqlQueryBuilder queryBuilder):
        m_queryBuilder { std::move(queryBuilder) }
    {
    }

  public:
    explicit RecordQueryBuilder():
        m_queryBuilder { SqlQueryBuilder::From(TargetModel().TableName()) }
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
        auto const sqlQueryString = m_queryBuilder.Count().ToSql(stmt.Connection().QueryFormatter());
        auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
        return stmt.ExecuteDirectScalar<size_t>(sqlQueryString).value_or(0);
    }

    [[nodiscard]] std::optional<TargetModel> First(size_t count = 1)
    {
        TargetModel targetRecord;

        auto stmt = SqlStatement {};

        auto const sqlQueryString = m_queryBuilder.Select(targetRecord.AllFieldNames(), targetRecord.TableName())
                                        .First(count)
                                        .ToSql(stmt.Connection().QueryFormatter());

        auto const _ = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

        if (auto result = stmt.Prepare(sqlQueryString); !result)
            return std::nullopt;

        if (auto result = stmt.Execute(); !result)
            return std::nullopt;

        if (auto result = stmt.BindOutputColumn(1, &targetRecord.MutableId().value); !result)
            return std::nullopt;

        for (AbstractField* field: targetRecord.AllFields())
            if (auto result = field->BindOutputColumn(stmt); !result)
                return std::nullopt;

        if (auto result = stmt.FetchRow(); !result)
            return std::nullopt;

        return { std::move(targetRecord) };
    }

    [[nodiscard]] std::vector<TargetModel> Range(std::size_t offset, std::size_t limit)
    {
        auto const targetRecord = TargetModel();
        auto const sqlQueryString = m_queryBuilder.Select(targetRecord.AllFieldNames(), targetRecord.TableName())
                                        .Range(offset, limit)
                                        .ToSql(SqlConnection().QueryFormatter());
        return TargetModel::Query(sqlQueryString).value_or(std::vector<TargetModel> {});
    }

    template <typename Callback>
    bool Each(Callback&& callback)
    {
        auto const targetRecord = TargetModel();
        auto const sqlQueryString = m_queryBuilder.Select(targetRecord.AllFieldNames(), targetRecord.TableName())
                                        .All()
                                        .ToSql(SqlConnection().QueryFormatter());
        return TargetModel::Each(std::forward<Callback>(callback), sqlQueryString).has_value();
    }

    [[nodiscard]] std::vector<TargetModel> All()
    {
        auto const targetRecord = TargetModel();
        auto const sqlQueryString = m_queryBuilder.Select(targetRecord.AllFieldNames(), targetRecord.TableName())
                                        .All()
                                        .ToSql(SqlConnection().QueryFormatter());
        return TargetModel::Query(sqlQueryString).value_or(std::vector<TargetModel> {});
    }

  private:
    SqlQueryBuilder m_queryBuilder;
};

template <typename Derived>
struct Record: AbstractRecord
{
  public:
    Record() = delete;
    Record(Record const&) = default;
    Record& operator=(Record const&) = delete;
    Record& operator=(Record&&) = default;
    ~Record() = default;

    Record(Record&& other) noexcept:
        AbstractRecord(std::move(other))
    {
    }

    // Creates (or recreates a copy of) the model in the database.
    SqlResult<RecordId> Create();

    // Reads the model from the database by given model ID.
    SqlResult<void> Load(RecordId id);

    // Re-reads the model from the database.
    SqlResult<void> Reload();

    // Reads the model from the database by given column name and value.
    template <typename T>
    SqlResult<void> Load(std::string_view const& columnName, T const& value) noexcept;

    // Updates the model in the database.
    SqlResult<void> Update();

    // Creates or updates the model in the database, depending on whether it already exists.
    SqlResult<void> Save();

    // Deletes the model from the database.
    SqlResult<void> Destroy();

    // Updates all models with the given changes in the modelChanges model.
    static SqlResult<void> UpdateAll(Derived const& modelChanges) noexcept;

    // Retrieves the first model from the database (ordered by ID ASC).
    static SqlResult<Derived> First(size_t count = 1);

    // Retrieves the last model from the database (ordered by ID ASC).
    static SqlResult<Derived> Last();

    // Retrieves the model with the given ID from the database.
    static SqlResult<Derived> Find(RecordId id);

    template <typename ColumnName, typename T>
    static SqlResult<Derived> FindBy(ColumnName const& columnName, T const& value);

    // Retrieves all models of this kind from the database.
    static SqlResult<std::vector<Derived>> All() noexcept;

    // Retrieves the number of models of this kind from the database.
    static SqlResult<size_t> Count() noexcept;

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
    static SqlResult<void> Each(Callback&& callback,
                                std::string_view sqlQueryString,
                                InputParameters&&... inputParameters);

    template <typename... InputParameters>
    static SqlResult<std::vector<Derived>> Query(std::string_view sqlQueryString, InputParameters&&... inputParameters);

    // Returns the SQL string to create the table for this model.
    static std::string CreateTableString(SqlServerType serverType);

    // Creates the table for this model from the database.
    static SqlResult<void> CreateTable();

    // Drops the table for this model from the database.
    static SqlResult<void> DropTable();

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
SqlResult<size_t> Record<Derived>::Count() noexcept
{
    SqlStatement stmt;
    Derived modelSchema;

    return stmt.Prepare(std::format("SELECT COUNT(*) FROM {}", modelSchema.TableName()))
        .and_then([&] { return stmt.Execute(); })
        .and_then([&] { return stmt.FetchRow(); })
        .and_then([&] { return stmt.GetColumn<size_t>(1); });
}

template <typename Derived>
SqlResult<Derived> Record<Derived>::Find(RecordId id)
{
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for Find() to return the model.");
    Derived model;
    return model.Load(id).and_then([&]() -> SqlResult<Derived> { return { std::move(model) }; });
}

template <typename Derived>
template <typename ColumnName, typename T>
SqlResult<Derived> Record<Derived>::FindBy(ColumnName const& columnName, T const& value)
{
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for Find() to return the model.");
    Derived model;
    return model.Load(columnName, value).and_then([&]() -> SqlResult<Derived> { return { std::move(model) }; });
}

template <typename Derived>
SqlResult<std::vector<Derived>> Record<Derived>::All() noexcept
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

    if (auto result = stmt.Prepare(sqlQueryString); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    while (true)
    {
        Derived record;

        if (auto result = stmt.BindOutputColumn(1, &record.m_data->id.value); !result)
            return std::unexpected { result.error() };

        for (AbstractField* field: record.AllFields())
            if (auto result = field->BindOutputColumn(stmt); !result)
                return std::unexpected { result.error() };

        if (auto result = stmt.FetchRow(); !result)
            break;

        scopedModelSqlLogger += record;

        allModels.emplace_back(std::move(record));
    }

    if (stmt.LastError() != SqlError::NO_DATA_FOUND)
        return std::unexpected { stmt.LastError() };

    return { std::move(allModels) };
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

    return std::move(*sql);
}

template <typename Derived>
SqlResult<void> Record<Derived>::CreateTable()
{
    auto stmt = SqlStatement {};
    auto const sqlQueryString = CreateTableString(stmt.Connection().ServerType());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    return stmt.ExecuteDirect(sqlQueryString);
}

template <typename Derived>
SqlResult<void> Record<Derived>::DropTable()
{
    auto const sqlQueryString = std::format("DROP TABLE \"{}\"", Derived().TableName());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    return SqlStatement().ExecuteDirect(sqlQueryString);
}

template <typename Derived>
SqlResult<RecordId> Record<Derived>::Create()
{
    auto const requiredFieldCount =
        std::ranges::count_if(AllFields(), [](AbstractField const* field) { return field->IsRequired(); });

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

    if (auto result = stmt.Prepare(sqlInsertStmtString); !result)
        return std::unexpected { result.error() };

    for (auto const&& [parameterIndex, field]: modifiedFields | std::views::enumerate)
        if (auto result = field->BindInputParameter(parameterIndex + 1, stmt); !result)
            return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    for (auto* field: AllFields())
        field->SetModified(false);

    // Update the model's ID with the last insert ID
    if (auto const result = stmt.LastInsertId(); result)
        m_data->id = RecordId { .value = *result };

    return {};
}

template <typename Derived>
SqlResult<void> Record<Derived>::Load(RecordId id)
{
    return Load(PrimaryKeyName(), id.value);
}

template <typename Derived>
SqlResult<void> Record<Derived>::Reload()
{
    return Load(PrimaryKeyName(), Id());
}

template <typename Derived>
template <typename T>
SqlResult<void> Record<Derived>::Load(std::string_view const& columnName, T const& value) noexcept
{
    SqlStatement stmt;

    auto const sqlQueryString = SqlQueryBuilder::From(TableName())
                                    .Select(AllFieldNames())
                                    .Where(columnName, SqlQueryWildcard())
                                    .First()
                                    .ToSql(stmt.Connection().QueryFormatter());

    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, AllFields());

    if (auto result = stmt.Prepare(sqlQueryString); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.BindInputParameter(1, value); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.BindOutputColumn(1, &m_data->id.value); !result)
        return std::unexpected { result.error() };

    for (AbstractField* field: AllFields())
        if (auto result = field->BindOutputColumn(stmt); !result)
            return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.FetchRow(); !result)
        return std::unexpected { result.error() };

    return {};
}

template <typename Derived>
SqlResult<void> Record<Derived>::Update()
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
SqlResult<void> Record<Derived>::Save()
{
    if (Id().value != 0)
        return Update();

    if (auto result = Create(); !result)
        return std::unexpected { result.error() };

    return {};
}

template <typename Derived>
SqlResult<void> Record<Derived>::Destroy()
{
    auto const sqlQueryString = std::format("DELETE FROM {} WHERE {} = {}", TableName(), PrimaryKeyName(), *Id());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    auto stmt = SqlStatement {};
    auto const& sqlTraits = stmt.Connection().Traits();
    return stmt.ExecuteDirect(sqlTraits.EnforceForeignKeyConstraint).and_then([&] {
        return stmt.ExecuteDirect(sqlQueryString);
    });
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
SqlResult<std::vector<Derived>> Record<Derived>::Query(std::string_view sqlQueryString,
                                                       InputParameters&&... inputParameters)
{
    static_assert(std::is_move_constructible_v<Derived>,
                  "The model `Derived` must be move constructible for Where() to return the models.");

    std::vector<Derived> output;
    auto result = Each([&output](Derived& model) { output.emplace_back(std::move(model)); },
                       sqlQueryString,
                       std::forward<InputParameters>(inputParameters)...);
    if (result.has_value())
        return { std::move(output) };
    else
        return std::unexpected { result.error() };
}

template <typename Derived>
template <typename Callback, typename... InputParameters>
SqlResult<void> Record<Derived>::Each(Callback&& callback,
                                      std::string_view sqlQueryString,
                                      InputParameters&&... inputParameters)
{
    SqlStatement stmt;

    auto scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    if (auto result = stmt.Prepare(sqlQueryString); !result)
        return std::unexpected { result.error() };

    SQLSMALLINT inputParameterPosition = 0;
    if (!(stmt.BindInputParameter(++inputParameterPosition, std::forward<InputParameters>(inputParameters)) && ...))
        return std::unexpected { stmt.LastError() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    while (true)
    {
        Derived record;

        if (auto result = stmt.BindOutputColumn(1, &record.m_data->id.value); !result)
            return std::unexpected { result.error() };

        for (AbstractField* field: record.AllFields())
            if (auto result = field->BindOutputColumn(stmt); !result)
                return std::unexpected { result.error() };

        if (auto result = stmt.FetchRow(); !result)
            break;

        scopedModelSqlLogger += record;

        callback(record);
    }

    if (stmt.LastError() != SqlError::NO_DATA_FOUND)
        return std::unexpected { stmt.LastError() };

    return {};
}

// }}}

} // namespace Model
