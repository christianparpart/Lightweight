#pragma once

#include "SqlDataBinder.hpp"

#include <concepts>
#include <cstdint>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

enum class SqlResultOrdering : uint8_t
{
    ASCENDING,
    DESCENDING
};

enum class SqlQueryType : uint8_t
{
    UNDEFINED,

    SELECT_ALL,
    SELECT_FIRST,
    SELECT_RANGE,
    SELECT_COUNT,

    // TODO: INSERT, // <-- is this a use-case we need?
    // TODO: UPDATE, // <-- is this a use-case we need?
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
    std::vector<SqlVariant> inputBindings;
    std::string tableJoins;
    std::string condition;
    std::vector<std::tuple<SqlQualifiedTableColumnName /*LHS*/, std::string_view /*op*/, bool /*RHS*/>>
        booleanLiteralConditions;
    std::string orderBy;
    std::string groupBy;
    size_t offset = 0;
    size_t limit = (std::numeric_limits<size_t>::max)();

    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;
};

class [[nodiscard]] SqlQueryBuilder
{
  public:
    static SqlQueryBuilder From(std::string_view table);

    // Adds a single column to the SELECT clause.
    [[nodiscard]] SqlQueryBuilder& Select(std::vector<std::string_view> const& fieldNames);

    // Adds a sequence of columns from the given table to the SELECT clause.
    [[nodiscard]] SqlQueryBuilder& Select(std::vector<std::string_view> const& fieldNames, std::string_view tableName);

    // Adds a sequence of columns to the SELECT clause.
    template <typename... MoreFields>
    [[nodiscard]] SqlQueryBuilder& Select(std::string_view const& firstField, MoreFields&&... moreFields);

    // Constructs or extends a raw WHERE clause.
    [[nodiscard]] SqlQueryBuilder& Where(std::string_view sqlConditionExpression);

    // Constructs or extends a WHERE clause to test for a binary operation.
    template <typename ColumnName, typename T>
    [[nodiscard]] SqlQueryBuilder& Where(ColumnName const& columnName, std::string_view binaryOp, T const& value);

    // Constructs or extends a WHERE clause to test for equality.
    template <typename ColumnName, typename T>
    [[nodiscard]] SqlQueryBuilder& Where(ColumnName const& columnName, T const& value);

    // Constructs or extends a WHERE clause to test for a range of values.
    template <typename ColumnName, std::ranges::input_range InputRange>
    [[nodiscard]] SqlQueryBuilder& Where(ColumnName const& columnName, InputRange&& values);

    // Constructs or extends a ORDER BY clause.
    [[nodiscard]] SqlQueryBuilder& OrderBy(std::string_view columnName,
                                           SqlResultOrdering ordering = SqlResultOrdering::ASCENDING);

    // Constructs or extends a GROUP BY clause.
    [[nodiscard]] SqlQueryBuilder& GroupBy(std::string_view columnName);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] SqlQueryBuilder& InnerJoin(std::string_view joinTable,
                                             std::string_view joinColumnName,
                                             SqlQualifiedTableColumnName onOtherColumn);

    // Constructs an INNER JOIN clause.
    [[nodiscard]] SqlQueryBuilder& InnerJoin(std::string_view joinTable,
                                             std::string_view joinColumnName,
                                             std::string_view onMainTableColumn);

    // final methods

    // Finalizes building the query as SELECT COUNT(*) ... query.
    SqlComposedQuery Count();

    // Finalizes building the query as SELECT field names FROM ... query.
    SqlComposedQuery All();

    // Finalizes building the query as SELECT TOP n field names FROM ... query.
    SqlComposedQuery First(size_t count = 1);

    // Finalizes building the query as SELECT field names FROM ... query with a range.
    SqlComposedQuery Range(std::size_t offset, std::size_t limit);

    // Finalizes building the query as DELETE FROM ... query.
    SqlComposedQuery Delete();

  private:
    explicit SqlQueryBuilder(std::string_view table);

    SqlComposedQuery m_query {};
};

// {{{ SqlQueryBuilder template implementations and inlines

template <typename... MoreFields>
SqlQueryBuilder& SqlQueryBuilder::Select(std::string_view const& firstField, MoreFields&&... moreFields)
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

template <typename ColumnName, typename T>
SqlQueryBuilder& SqlQueryBuilder::Where(ColumnName const& columnName, T const& value)
{
    return Where(columnName, "=", value);
}

template <typename ColumnName, std::ranges::input_range InputRange>
SqlQueryBuilder& SqlQueryBuilder::Where(ColumnName const& columnName, InputRange&& values)
{
    return Where(columnName, "IN", [](auto const& values) {
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
        return detail::RawSqlCondition { fragment.str() };
    }(std::forward<InputRange>(values)));
}

template <typename T>
struct WhereConditionLiteralType
{
    constexpr static bool needsQuotes = !std::is_integral_v<T> && !std::is_floating_point_v<T>;
};

template <typename ColumnName, typename T>
SqlQueryBuilder& SqlQueryBuilder::Where(ColumnName const& columnName, std::string_view binaryOp, T const& value)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        if constexpr (std::is_same_v<ColumnName, SqlQualifiedTableColumnName>)
            m_query.booleanLiteralConditions.emplace_back(columnName, binaryOp, value);
        else
            m_query.booleanLiteralConditions.emplace_back(
                SqlQualifiedTableColumnName { "", columnName }, binaryOp, value);
        return *this;
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

    return *this;
}

// }}}
