#pragma once

#include "SqlDataBinder.hpp"

#include <concepts>
#include <cstdint>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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

struct [[nodiscard]] SqlSearchCondition
{
    std::string tableName;
    std::string tableAlias;
    std::string tableJoins;
    std::string condition;
    std::vector<std::tuple<SqlQualifiedTableColumnName /*LHS*/, std::string_view /*op*/, bool /*RHS*/>>
        booleanLiteralConditions;
    std::vector<SqlVariant> inputBindings;

    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;
};

namespace detail
{

template <typename Derived>
class [[nodiscard]] SqlWhereClauseBuilder
{
  public:
    // Constructs or extends a raw WHERE clause.
    [[nodiscard]] Derived& Where(std::string_view sqlConditionExpression);

    // Constructs or extends a WHERE clause to test for a binary operation.
    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& Where(ColumnName const& columnName, std::string_view binaryOp, T const& value);

    // Constructs or extends a WHERE clause to test for equality.
    template <typename ColumnName, typename T>
    [[nodiscard]] Derived& Where(ColumnName const& columnName, T const& value);

    // Constructs or extends a WHERE/AND clause to test for a group of values.
    template <typename Callable>
        requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
    [[nodiscard]] Derived& Where(Callable&& callable);

    // Constructs or extends an WHERE/OR clause to test for a group of values.
    template <typename Callable>
        requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
    [[nodiscard]] Derived& OrWhere(Callable&& callable);

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

    // Construts or extends a WHERE clause to test for a binary operation between two columns.
    template <typename LeftColumn, typename RightColumn>
    [[nodiscard]] Derived& WhereColumn(LeftColumn const& left, std::string_view binaryOp, RightColumn const& right);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& InnerJoin(std::string_view joinTable,
                                     std::string_view joinColumnName,
                                     SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] Derived& InnerJoin(std::string_view joinTable,
                                     std::string_view joinColumnName,
                                     std::string_view onMainTableColumn);

    // Constructs an LEFT OUTER JOIN clause.
    [[nodiscard]] Derived& LeftOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an LEFT OUTER JOIN clause.
    [[nodiscard]] Derived& LeftOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         std::string_view onMainTableColumn);

    // Constructs an RIGHT OUTER JOIN clause.
    [[nodiscard]] Derived& RightOuterJoin(std::string_view joinTable,
                                          std::string_view joinColumnName,
                                          SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an RIGHT OUTER JOIN clause.
    [[nodiscard]] Derived& RightOuterJoin(std::string_view joinTable,
                                          std::string_view joinColumnName,
                                          std::string_view onMainTableColumn);

    // Constructs an FULL OUTER JOIN clause.
    [[nodiscard]] Derived& FullOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an FULL OUTER JOIN clause.
    [[nodiscard]] Derived& FullOuterJoin(std::string_view joinTable,
                                         std::string_view joinColumnName,
                                         std::string_view onMainTableColumn);

  private:
    SqlSearchCondition& SearchCondition() noexcept;

    enum class WhereJunctor : uint8_t
    {
        Null,
        Where,
        And,
        Or,
    };

    WhereJunctor m_nextWhereJunctor = WhereJunctor::Where;

    void AppendWhereJunctor();

    template <typename ColumnName>
        requires(std::is_same_v<ColumnName, SqlQualifiedTableColumnName>
                 || std::is_convertible_v<ColumnName, std::string_view>
                 || std::is_convertible_v<ColumnName, std::string>)
    void AppendColumnName(ColumnName const& columnName);

    enum class JoinType : uint8_t
    {
        INNER,
        LEFT,
        RIGHT,
        FULL
    };

    // Constructs a JOIN clause.
    [[nodiscard]] Derived& Join(JoinType joinType,
                                std::string_view joinTable,
                                std::string_view joinColumnName,
                                SqlQualifiedTableColumnName onOtherColumn);

    // Constructs a JOIN clause.
    [[nodiscard]] Derived& Join(JoinType joinType,
                                std::string_view joinTable,
                                std::string_view joinColumnName,
                                std::string_view onMainTableColumn);
};

} // namespace detail

class [[nodiscard]] SqlInsertQueryBuilder final
{
  public:
    explicit SqlInsertQueryBuilder(std::string tableName, std::vector<SqlVariant>* boundInputs) noexcept:
        m_tableName { std::move(tableName) },
        m_boundInputs { boundInputs }
    {
    }

    // Adds a single column to the INSERT query.
    template <typename ColumnValue>
    SqlInsertQueryBuilder& Set(std::string_view columnName, ColumnValue const& value);

    // Finalizes building the query as INSERT INTO ... query.
    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;

  private:
    std::string m_tableName;
    std::string m_fields;
    std::string m_values;
    std::vector<SqlVariant>* m_boundInputs;
};

enum class SqlResultOrdering : uint8_t
{
    ASCENDING,
    DESCENDING
};

class [[nodiscard]] SqlSelectQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder>
{
  public:
    enum class SelectType
    {
        Undefined,
        Count,
        All,
        First,
        Range
    };

    struct ComposedQuery
    {
        SelectType selectType = SelectType::Undefined;
        bool distinct = false;
        SqlSearchCondition searchCondition {};

        std::string fields;

        std::string orderBy;
        std::string groupBy;

        size_t offset = 0;
        size_t limit = (std::numeric_limits<size_t>::max)();

        [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;
    };

    explicit SqlSelectQueryBuilder(std::string table, std::string tableAlias) noexcept:
        detail::SqlWhereClauseBuilder<SqlSelectQueryBuilder> {}
    {
        m_query.searchCondition.tableName = std::move(table);
        m_query.searchCondition.tableAlias = std::move(tableAlias);
        m_query.fields.reserve(256);
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
    ComposedQuery Count();

    // Finalizes building the query as SELECT field names FROM ... query.
    ComposedQuery All();

    // Finalizes building the query as SELECT TOP n field names FROM ... query.
    ComposedQuery First(size_t count = 1);

    // Finalizes building the query as SELECT field names FROM ... query with a range.
    ComposedQuery Range(std::size_t offset, std::size_t limit);

    SqlSearchCondition& SearchCondition() noexcept
    {
        return m_query.searchCondition;
    }

  private:
    ComposedQuery m_query;
};

class [[nodiscard]] SqlUpdateQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlUpdateQueryBuilder>
{
  public:
    SqlUpdateQueryBuilder(std::string table, std::string tableAlias, std::vector<SqlVariant>* boundInputs) noexcept:
        detail::SqlWhereClauseBuilder<SqlUpdateQueryBuilder> {},
        m_boundInputs { boundInputs }
    {
        m_searchCondition.tableName = std::move(table);
        m_searchCondition.tableAlias = std::move(tableAlias);
    }

    SqlSearchCondition& SearchCondition() noexcept
    {
        return m_searchCondition;
    }

    // Adds a single column to the SET clause.
    template <typename ColumnValue>
    SqlUpdateQueryBuilder& Set(std::string_view columnName, ColumnValue const& value);

    // Finalizes building the query as UPDATE ... query.
    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;

  private:
    std::string m_values;
    SqlSearchCondition m_searchCondition;
    std::vector<SqlVariant>* m_boundInputs;
};

class [[nodiscard]] SqlDeleteQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlDeleteQueryBuilder>
{
  public:
    explicit SqlDeleteQueryBuilder(std::string table, std::string tableAlias) noexcept:
        detail::SqlWhereClauseBuilder<SqlDeleteQueryBuilder> {}
    {
        m_searchCondition.tableName = std::move(table);
        m_searchCondition.tableAlias = std::move(tableAlias);
    }

    SqlSearchCondition& SearchCondition() noexcept
    {
        return m_searchCondition;
    }

    // Finalizes building the query as DELETE FROM ... query.
    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;

  private:
    SqlSearchCondition m_searchCondition;
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
    SqlUpdateQueryBuilder Update(std::vector<SqlVariant>* boundInputs) && noexcept;

    // Initiates DELETE query building
    SqlDeleteQueryBuilder Delete() && noexcept;

  private:
    explicit SqlQueryBuilder(std::string&& table) noexcept;
    explicit SqlQueryBuilder(std::string&& table, std::string&& alias) noexcept;

    std::string m_table;
    std::string m_tableAlias;
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

template <typename Derived>
template <typename Callable>
    requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
Derived& SqlWhereClauseBuilder<Derived>::OrWhere(Callable&& callable)
{
    if (m_nextWhereJunctor != WhereJunctor::Where)
        m_nextWhereJunctor = WhereJunctor::Or;
    return Where(std::forward<Callable>(callable));
}

template <typename Derived>
template <typename Callable>
    requires std::invocable<Callable, SqlWhereClauseBuilder<Derived>&>
Derived& SqlWhereClauseBuilder<Derived>::Where(Callable&& callable)
{
    auto& condition = SearchCondition().condition;

    auto const originalSize = condition.size();

    AppendWhereJunctor();
    m_nextWhereJunctor = WhereJunctor::Null;
    condition += '(';

    auto const sizeBeforeCallable = condition.size();

    (void) callable(*this);

    if (condition.size() == sizeBeforeCallable)
        condition.resize(originalSize);
    else
        condition += ')';

    return static_cast<Derived&>(*this);
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

template <typename Derived>
template <typename LeftColumn, typename RightColumn>
Derived& SqlWhereClauseBuilder<Derived>::WhereColumn(LeftColumn const& left,
                                                     std::string_view binaryOp,
                                                     RightColumn const& right)
{
    AppendWhereJunctor();

    AppendColumnName(left);

    SearchCondition().condition += ' ';
    SearchCondition().condition += binaryOp;
    SearchCondition().condition += ' ';

    AppendColumnName(right);

    return static_cast<Derived&>(*this);
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
    auto& searchCondition = SearchCondition();
    if constexpr (std::is_same_v<T, bool>)
    {
        if constexpr (std::is_same_v<ColumnName, SqlQualifiedTableColumnName>)
            searchCondition.booleanLiteralConditions.emplace_back(columnName, binaryOp, value);
        else
            searchCondition.booleanLiteralConditions.emplace_back(
                SqlQualifiedTableColumnName { "", columnName }, binaryOp, value);
        return static_cast<Derived&>(*this);
    }

    AppendWhereJunctor();

    AppendColumnName(columnName);

    searchCondition.condition += ' ';
    searchCondition.condition += binaryOp;
    searchCondition.condition += ' ';

    if constexpr (std::is_same_v<T, SqlQueryWildcard>)
    {
        searchCondition.condition += '?';
        searchCondition.inputBindings.emplace_back(SqlNullValue);
    }
    else if constexpr (std::is_same_v<T, detail::RawSqlCondition>)
    {
        searchCondition.condition += value.condition;
    }
    else if constexpr (!WhereConditionLiteralType<T>::needsQuotes)
    {
        searchCondition.condition += std::format("{}", value);
    }
    else
    {
        searchCondition.condition += '\'';
        searchCondition.condition += std::format("{}", value);
        searchCondition.condition += '\'';
        // TODO: This should be bound as an input parameter in the future instead.
        // searchCondition.inputBindings.emplace_back(value);
    }

    return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::InnerJoin(std::string_view joinTable,
                                                   std::string_view joinColumnName,
                                                   SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::INNER, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::InnerJoin(std::string_view joinTable,
                                                   std::string_view joinColumnName,
                                                   std::string_view onMainTableColumn)
{
    return Join(JoinType::INNER, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::LeftOuterJoin(std::string_view joinTable,
                                                       std::string_view joinColumnName,
                                                       SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::LEFT, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::LeftOuterJoin(std::string_view joinTable,
                                                       std::string_view joinColumnName,
                                                       std::string_view onMainTableColumn)
{
    return Join(JoinType::LEFT, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::RightOuterJoin(std::string_view joinTable,
                                                        std::string_view joinColumnName,
                                                        SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::RIGHT, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::RightOuterJoin(std::string_view joinTable,
                                                        std::string_view joinColumnName,
                                                        std::string_view onMainTableColumn)
{
    return Join(JoinType::RIGHT, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::FullOuterJoin(std::string_view joinTable,
                                                       std::string_view joinColumnName,
                                                       SqlQualifiedTableColumnName onOtherColumn)
{
    return Join(JoinType::FULL, joinTable, joinColumnName, onOtherColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::FullOuterJoin(std::string_view joinTable,
                                                       std::string_view joinColumnName,
                                                       std::string_view onMainTableColumn)
{
    return Join(JoinType::FULL, joinTable, joinColumnName, onMainTableColumn);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::Where(std::string_view sqlConditionExpression)
{
    auto& condition = SearchCondition().condition;

    AppendWhereJunctor();

    condition += '(';
    condition += std::string(sqlConditionExpression);
    condition += ')';

    return static_cast<Derived&>(*this);
}

template <typename Derived>
inline SqlSearchCondition& SqlWhereClauseBuilder<Derived>::SearchCondition() noexcept
{
    return static_cast<Derived*>(this)->SearchCondition();
}

template <typename Derived>
void SqlWhereClauseBuilder<Derived>::AppendWhereJunctor()
{
    using namespace std::string_view_literals;

    auto& condition = SearchCondition().condition;

    switch (m_nextWhereJunctor)
    {
        case WhereJunctor::Null:
            break;
        case WhereJunctor::Where:
            condition += "\n WHERE "sv;
            break;
        case WhereJunctor::And:
            condition += " AND "sv;
            break;
        case WhereJunctor::Or:
            condition += " OR "sv;
            break;
    }

    m_nextWhereJunctor = WhereJunctor::And;
}

template <typename Derived>
template <typename ColumnName>
    requires(std::is_same_v<ColumnName, SqlQualifiedTableColumnName>
             || std::is_convertible_v<ColumnName, std::string_view> || std::is_convertible_v<ColumnName, std::string>)
void SqlWhereClauseBuilder<Derived>::AppendColumnName(ColumnName const& columnName)
{
    using namespace std::string_view_literals;

    auto& condition = SearchCondition().condition;
    if constexpr (std::is_same_v<ColumnName, SqlQualifiedTableColumnName>)
    {
        condition += '"';
        condition += columnName.tableName;
        condition += R"(".")"sv;
        condition += columnName.columnName;
        condition += '"';
    }
    else
    {
        condition += '"';
        condition += columnName;
        condition += '"';
    }
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::Join(JoinType joinType,
                                              std::string_view joinTable,
                                              std::string_view joinColumnName,
                                              SqlQualifiedTableColumnName onOtherColumn)
{
    static constexpr std::array<std::string_view, 4> JoinTypeStrings = {
        "INNER",
        "LEFT OUTER",
        "RIGHT OUTER",
        "FULL OUTER",
    };

    SearchCondition().tableJoins += std::format("\n"
                                                R"( {0} JOIN "{1}" ON "{1}"."{2}" = "{3}"."{4}")",
                                                JoinTypeStrings[static_cast<std::size_t>(joinType)],
                                                joinTable,
                                                joinColumnName,
                                                onOtherColumn.tableName,
                                                onOtherColumn.columnName);
    return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived& SqlWhereClauseBuilder<Derived>::Join(JoinType joinType,
                                              std::string_view joinTable,
                                              std::string_view joinColumnName,
                                              std::string_view onMainTableColumn)
{
    return Join(
        joinType,
        joinTable,
        joinColumnName,
        SqlQualifiedTableColumnName { .tableName = SearchCondition().tableName, .columnName = onMainTableColumn });
}

} // namespace detail
// }}}

// {{{ SqlInsertQueryBuilder impl
template <typename ColumnValue>
SqlInsertQueryBuilder& SqlInsertQueryBuilder::Set(std::string_view columnName, ColumnValue const& value)
{
    using namespace std::string_view_literals;

    if (!m_fields.empty())
        m_fields += ", "sv;

    m_fields += '"';
    m_fields += columnName;
    m_fields += '"';

    if (!m_values.empty())
        m_values += ", "sv;

    if constexpr (std::is_same_v<ColumnValue, SqlNullType>)
        m_values += "NULL"sv;
    else if constexpr (std::is_arithmetic_v<ColumnValue>)
        m_values += std::format("{}", value);
    else if constexpr (std::is_same_v<ColumnValue, SqlQueryWildcard>)
    {
        m_values += '?';
        m_boundInputs->emplace_back(SqlNullValue);
    }
    else
    {
        m_values += '?';
        m_boundInputs->emplace_back(value);
    }

    return *this;
}

template <typename ColumnValue>
SqlUpdateQueryBuilder& SqlUpdateQueryBuilder::Set(std::string_view columnName, ColumnValue const& value)
{
    using namespace std::string_view_literals;

    if (!m_values.empty())
        m_values += ", "sv;

    m_values += '"';
    m_values += columnName;
    m_values += R"(" = )"sv;

    if constexpr (std::is_same_v<ColumnValue, SqlNullType>)
        m_values += "NULL"sv;
    else
    {
        m_values += '?';
        m_boundInputs->emplace_back(value);
    }

    return *this;
}
// }}}

// {{{ SqlSelectQueryBuilder impl
template <typename... MoreFields>
SqlSelectQueryBuilder& SqlSelectQueryBuilder::Fields(std::string_view const& firstField, MoreFields&&... moreFields)
{
    using namespace std::string_view_literals;

    std::ostringstream fragment;

    if (!m_query.fields.empty())
        fragment << ", "sv;

    fragment << '"' << firstField << '"';

    if constexpr (sizeof...(MoreFields) > 0)
        ((fragment << R"(, ")"sv << std::forward<MoreFields>(moreFields) << '"') << ...);

    m_query.fields += fragment.str();
    return *this;
}
// }}}

// {{{ SqlQueryBuilder impl
inline SqlQueryBuilder::SqlQueryBuilder(std::string&& table) noexcept:
    m_table { std::move(table) }
{
}

inline SqlQueryBuilder::SqlQueryBuilder(std::string&& table, std::string&& alias) noexcept:
    m_table { std::move(table) },
    m_tableAlias { std::move(alias) }
{
}
// }}}
