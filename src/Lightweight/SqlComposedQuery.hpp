#pragma once

#include "SqlDataBinder.hpp"

#include <concepts>
#include <cstdint>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

enum class SqlResultOrdering : uint8_t
{
    ASCENDING,
    DESCENDING
};

enum class SqlQueryType : uint8_t
{
    UNDEFINED,

    INSERT,

    SELECT_ALL,
    SELECT_FIRST,
    SELECT_RANGE,
    SELECT_COUNT,

    UPDATE,
    DELETE_ // "DELETE" is ABUSED by winnt.h on Windows as preprocessor definition. Thanks!
};

// SqlQueryWildcard is a placeholder for an explicit wildcard input parameter in a SQL query.
//
// Use this in the SqlQueryBuilder::Where method to insert a '?' placeholder for a wildcard.
struct SqlQueryWildcard
{
};

namespace detail
{

struct RawSqlCondition
{
    std::string condition;
};

} // namespace detail

struct SqlQualifiedTableColumnName
{
    std::string_view tableName;
    std::string_view columnName;
};

class SqlQueryFormatter;

struct [[nodiscard]] SqlComposedQuery
{
    SqlQueryType type = SqlQueryType::UNDEFINED;
    std::string fields;
    std::string table;
    std::string tableAlias;
    std::string inputValues;
    std::vector<SqlVariant> inputBindings;
    std::string tableJoins;
    std::string condition;
    std::vector<std::tuple<SqlQualifiedTableColumnName /*LHS*/, std::string_view /*op*/, bool /*RHS*/>>
        booleanLiteralConditions;
    std::string orderBy;
    std::string groupBy;
    size_t offset = 0;
    size_t limit = (std::numeric_limits<size_t>::max)();
    bool distinct = false; // If true and a select query, then SELECT DISTINCT is used.

    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;
};

enum class SqlJoinType : uint8_t
{
    INNER,
    LEFT,
    RIGHT,
    FULL
};

namespace detail
{

template <typename Derived>
class [[nodiscard]] SqlWhereClauseBuilder
{
  public:
    explicit SqlWhereClauseBuilder(SqlComposedQuery query = {}):
        m_query(std::move(query))
    {
    }

    // Constructs or extends a raw WHERE clause.
    [[nodiscard]] Derived& Where(std::string_view sqlConditionExpression);

    // Constructs or extends a WHERE clause to test for a binary operation.
    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& Where(ColumnName const& columnName, std::string_view binaryOp, T const& value);

    // Constructs or extends a WHERE clause to test for equality.
    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& Where(ColumnName const& columnName, T const& value);

    template <typename ColumnName, std::ranges::input_range InputRange>
    [[nodiscard]] Derived& WhereIn(ColumnName const& columnName, InputRange&& values);

    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& WhereIn(ColumnName const& columnName, std::initializer_list<T>&& values);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereNull(ColumnName const& columnName);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereNotNull(ColumnName const& columnName);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereTrue(ColumnName const& columnName);

    template <typename ColumnName>
    [[nodiscard]] Derived& WhereFalse(ColumnName const& columnName);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& Join(SqlJoinType joinType,
                                std::string_view joinTable,
                                std::string_view joinColumnName,
                                SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& Join(SqlJoinType joinType,
                                std::string_view joinTable,
                                std::string_view joinColumnName,
                                std::string_view onMainTableColumn);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& InnerJoin(std::string_view joinTable,
                                     std::string_view joinColumnName,
                                     SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& InnerJoin(std::string_view joinTable,
                                     std::string_view joinColumnName,
                                     std::string_view onMainTableColumn);

  protected:
    SqlComposedQuery m_query;
};

} // namespace detail

class [[nodiscard]] SqlInsertQueryBuilder final
{
  public:
    explicit SqlInsertQueryBuilder(SqlComposedQuery&& query, std::vector<SqlVariant>* boundInputs) noexcept:
        m_query { std::move(query) },
        m_boundInputs { boundInputs }
    {
    }

    // Adds a single column to the INSERT query.
    template <typename ColumnValue>
    SqlInsertQueryBuilder& Set(std::string_view columnName, ColumnValue const& value);

    // Finalizes building the query as INSERT INTO ... query.
    [[nodiscard]] SqlComposedQuery Build() noexcept;

  private:
    SqlComposedQuery m_query;
    std::vector<SqlVariant>* m_boundInputs;
};

class [[nodiscard]] SqlSelectQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder>
{
  public:
    explicit SqlSelectQueryBuilder(SqlComposedQuery&& query) noexcept:
        detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder> { std::move(query) }
    {
    }

    // Adds a DISTINCT clause to the SELECT query.
    SqlSelectQueryBuilder& Distinct() noexcept;

    // Adds a sequence of columns to the SELECT clause.
    template <typename... MoreFields>
    SqlSelectQueryBuilder& Fields(std::string_view const& firstField, MoreFields&&... moreFields);

    // Adds a single column to the SELECT clause.
    SqlSelectQueryBuilder& Field(std::string_view const& fieldName);

    // Adds a single column to the SELECT clause.
    SqlSelectQueryBuilder& Field(SqlQualifiedTableColumnName const& fieldName);

    // Adds a single column to the SELECT clause.
    SqlSelectQueryBuilder& Fields(std::vector<std::string_view> const& fieldNames);

    // Adds a sequence of columns from the given table to the SELECT clause.
    SqlSelectQueryBuilder& Fields(std::vector<std::string_view> const& fieldNames, std::string_view tableName);

    // Adds a single column with an alias to the SELECT clause.
    SqlSelectQueryBuilder& FieldAs(std::string_view const& fieldName, std::string_view const& alias);

    // Adds a single column with an alias to the SELECT clause.
    SqlSelectQueryBuilder& FieldAs(SqlQualifiedTableColumnName const& fieldName, std::string_view const& alias);

    // Constructs or extends a ORDER BY clause.
    SqlSelectQueryBuilder& OrderBy(std::string_view columnName,
                                   SqlResultOrdering ordering = SqlResultOrdering::ASCENDING);

    // Constructs or extends a GROUP BY clause.
    SqlSelectQueryBuilder& GroupBy(std::string_view columnName);

    // Finalizes building the query as SELECT COUNT(*) ... query.
    SqlComposedQuery Count();

    // Finalizes building the query as SELECT field names FROM ... query.
    SqlComposedQuery All();

    // Finalizes building the query as SELECT TOP n field names FROM ... query.
    SqlComposedQuery First(size_t count = 1);

    // Finalizes building the query as SELECT field names FROM ... query with a range.
    SqlComposedQuery Range(std::size_t offset, std::size_t limit);
};

class [[nodiscard]] SqlUpdateQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder>
{
  public:
    SqlUpdateQueryBuilder(SqlComposedQuery&& query) noexcept:
        detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder> { std::move(query) }
    {
    }

    // Adds a single column to the SET clause.
    SqlUpdateQueryBuilder& Set(std::string_view columnName, std::string_view value);

    // Adds a single column to the SET clause.
    SqlUpdateQueryBuilder& Set(SqlQualifiedTableColumnName columnName, std::string_view value);

    // Finalizes building the query as UPDATE ... query.
    SqlComposedQuery Build();
};

class [[nodiscard]] SqlDeleteQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlDeleteQueryBuilder>
{
  public:
    explicit SqlDeleteQueryBuilder(SqlComposedQuery&& query) noexcept:
        detail::SqlWhereClauseBuilder<SqlDeleteQueryBuilder> { std::move(query) }
    {
    }

    // Finalizes building the query as DELETE FROM ... query.
    [[nodiscard]] SqlComposedQuery Build() noexcept;

    // Finalizes building the query as DELETE FROM ... query.
    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;
};

class [[nodiscard]] SqlQueryBuilder final
{
  public:
    // Constructs a new query builder for the given table.
    static SqlQueryBuilder FromTable(std::string_view table);

    // Constructs a new query builder for the given table with an alias.
    static SqlQueryBuilder FromTableAs(std::string_view table, std::string_view alias);

    // Initiates INSERT query building
    SqlInsertQueryBuilder Insert(std::vector<SqlVariant>* boundInputs) && noexcept;

    // Initiates SELECT query building
    SqlSelectQueryBuilder Select() && noexcept;

    // Initiates UPDATE query building
    // TODO: SqlUpdateQueryBuilder Update() && noexcept;

    // Initiates DELETE query building
    SqlDeleteQueryBuilder Delete() && noexcept;

  private:
    explicit SqlQueryBuilder(std::string table);
    explicit SqlQueryBuilder(std::string table, std::string alias);

    SqlComposedQuery m_query;
};

// {{{ detail::SqlWhereClauseBuilder
namespace detail
{

template <typename Derived>
template <typename ColumnName, typename T>
Derived& SqlWhereClauseBuilder<Derived>::Where(ColumnName const& columnName, T const& value)
{
    return Where(columnName, "=", value);
}

RawSqlCondition PopulateSqlSetExpression(auto const&& values)
{
    using namespace std::string_view_literals;
    std::ostringstream fragment;
    fragment << '(';
    for (auto const&& [index, value]: values | std::views::enumerate)
    {
        if (index > 0)
            fragment << ", "sv;
        fragment << value;
    }
    fragment << ')';
    return RawSqlCondition { fragment.str() };
}

template <typename Derived>
template <typename ColumnName, std::ranges::input_range InputRange>
Derived& SqlWhereClauseBuilder<Derived>::WhereIn(ColumnName const& columnName, InputRange&& values)
{
    return Where(columnName, "IN", detail::PopulateSqlSetExpression(std::forward<InputRange>(values)));
}

template <typename Derived>
template <typename ColumnName, typename T>
Derived& SqlWhereClauseBuilder<Derived>::WhereIn(ColumnName const& columnName, std::initializer_list<T>&& values)
{
    return Where(columnName, "IN", detail::PopulateSqlSetExpression(std::forward<std::initializer_list<T>>(values)));
}

template <typename Derived>
template <typename ColumnName>
Derived& SqlWhereClauseBuilder<Derived>::WhereNotNull(ColumnName const& columnName)
{
    return Where(columnName, "!=", "NULL");
}

template <typename Derived>
template <typename ColumnName>
Derived& SqlWhereClauseBuilder<Derived>::WhereNull(ColumnName const& columnName)
{
    return Where(columnName, "=", "NULL");
}

template <typename Derived>
template <typename ColumnName>
Derived& SqlWhereClauseBuilder<Derived>::WhereTrue(ColumnName const& columnName)
{
    return Where(columnName, "=", true);
}

template <typename Derived>
template <typename ColumnName>
Derived& SqlWhereClauseBuilder<Derived>::WhereFalse(ColumnName const& columnName)
{
    return Where(columnName, "=", false);
}

template <typename T>
struct WhereConditionLiteralType
{
    constexpr static bool needsQuotes = !std::is_integral_v<T> && !std::is_floating_point_v<T>;
};

template <typename Derived>
template <typename ColumnName, typename T>
Derived& SqlWhereClauseBuilder<Derived>::Where(ColumnName const& columnName, std::string_view binaryOp, T const& value)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        if constexpr (std::is_same_v<ColumnName, SqlQualifiedTableColumnName>)
            m_query.booleanLiteralConditions.emplace_back(columnName, binaryOp, value);
        else
            m_query.booleanLiteralConditions.emplace_back(
                SqlQualifiedTableColumnName { "", columnName }, binaryOp, value);
        return static_cast<Derived&>(*this);
    }

    if (m_query.condition.empty())
        m_query.condition += " WHERE ";
    else
        m_query.condition += " AND ";

    if constexpr (std::is_same_v<ColumnName, SqlQualifiedTableColumnName>)
    {
        m_query.condition += std::format(R"("{}"."{}")", columnName.tableName, columnName.columnName);
    }
    else
    {
        m_query.condition += "\"";
        m_query.condition += columnName;
        m_query.condition += "\"";
    }

    m_query.condition += " ";
    m_query.condition += binaryOp;
    m_query.condition += " ";

    if constexpr (std::is_same_v<T, SqlQueryWildcard>)
    {
        m_query.condition += "?";
        m_query.inputBindings.emplace_back(SqlNullValue);
    }
    else if constexpr (std::is_same_v<T, detail::RawSqlCondition>)
    {
        m_query.condition += value.condition;
    }
    else if constexpr (!WhereConditionLiteralType<T>::needsQuotes)
    {
        m_query.condition += std::format("{}", value);
    }
    else
    {
        m_query.condition += "'";
        m_query.condition += std::format("{}", value);
        m_query.condition += "'";
        // TODO: This should be bound as an input parameter in the future instead.
        // m_query.inputBindings.emplace_back(value);
    }

    return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::Join(SqlJoinType joinType,
                                              std::string_view joinTable,
                                              std::string_view joinColumnName,
                                              SqlQualifiedTableColumnName onOtherColumn)
{
    static constexpr std::array<std::string_view, 4> JoinTypeStrings = { "INNER", "LEFT", "RIGHT", "FULL" };

    m_query.tableJoins += std::format(R"( {0} JOIN "{1}" ON "{1}"."{2}" = "{3}"."{4}")",
                                      JoinTypeStrings[static_cast<std::size_t>(joinType)],
                                      joinTable,
                                      joinColumnName,
                                      onOtherColumn.tableName,
                                      onOtherColumn.columnName);
    return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::Join(SqlJoinType joinType,
                                              std::string_view joinTable,
                                              std::string_view joinColumnName,
                                              std::string_view onMainTableColumn)
{
    return Join(joinType,
                joinTable,
                joinColumnName,
                SqlQualifiedTableColumnName { .tableName = m_query.table, .columnName = onMainTableColumn });
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::InnerJoin(std::string_view joinTable,
                                                   std::string_view joinColumnName,
                                                   SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(SqlJoinType::INNER, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::InnerJoin(std::string_view joinTable,
                                                   std::string_view joinColumnName,
                                                   std::string_view onMainTableColumn)
{
    return Join(SqlJoinType::INNER, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::Where(std::string_view sqlConditionExpression)
{
    if (m_query.condition.empty())
        m_query.condition += " WHERE ";
    else
        m_query.condition += " AND ";

    m_query.condition += "(";
    m_query.condition += std::string(sqlConditionExpression);
    m_query.condition += ")";

    return static_cast<Derived&>(*this);
}

} // namespace detail
// }}}

// {{{ SqlInsertQueryBuilder impl
template <typename ColumnValue>
SqlInsertQueryBuilder& SqlInsertQueryBuilder::Set(std::string_view columnName, ColumnValue const& value)
{
    if (!m_query.fields.empty())
        m_query.fields += ", ";

    m_query.fields += '"';
    m_query.fields += columnName;
    m_query.fields += '"';

    if (!m_query.inputValues.empty())
        m_query.inputValues += ", ";

    if constexpr (std::is_same_v<ColumnValue, SqlNullType>)
        m_query.inputValues += "NULL";
    else if constexpr (std::is_same_v<ColumnValue, SqlQueryWildcard>)
    {
        m_query.inputValues += "?";
        m_boundInputs->emplace_back(SqlNullValue);
    }
    else
    {
        m_query.inputValues += "?";
        m_boundInputs->emplace_back(value);
    }

    return *this;
}

inline SqlComposedQuery SqlInsertQueryBuilder::Build() noexcept
{
    m_query.type = SqlQueryType::INSERT;
    return std::move(m_query);
}

// }}}

// {{{ SqlSelectQueryBuilder impl
template <typename... MoreFields>
SqlSelectQueryBuilder& SqlSelectQueryBuilder::Fields(std::string_view const& firstField, MoreFields&&... moreFields)
{
    std::ostringstream fragment;

    if (!m_query.fields.empty())
        fragment << ", ";

    fragment << "\"" << firstField << "\"";

    if constexpr (sizeof...(MoreFields) > 0)
        ((fragment << ", \"" << std::forward<MoreFields>(moreFields) << "\"") << ...);

    m_query.fields += fragment.str();
    return *this;
}
// }}}
