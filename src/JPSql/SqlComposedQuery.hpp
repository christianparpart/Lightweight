#pragma once

#include "SqlDataBinder.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

enum class SqlResultOrdering : uint8_t
{
    ASCENDING,
    DESCENDING
};

enum class SqlQueryType
{
    UNDEFINED,

    SELECT_ALL,
    SELECT_FIRST,
    SELECT_RANGE,
    SELECT_COUNT,

    // INSERT,
    // UPDATE,
    // DELETE -- ABUSED by winnt.h on Windows as preprocessor definition. Thanks!
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
    std::string orderBy;
    std::string groupBy;
    size_t offset = 0;
    size_t limit = std::numeric_limits<size_t>::max();

    [[nodiscard]] std::string ToSql(SqlQueryFormatter const& formatter) const;
};

// SqlQueryWildcard is a placeholder for an explicit wildcard input parameter in a SQL query.
//
// Use this in the SqlQueryBuilder::Where method to insert a '?' placeholder for a wildcard.
struct SqlQueryWildcard
{
};

struct SqlQualifiedTableColumnName
{
    std::string_view tableName;
    std::string_view columnName;
};

class [[nodiscard]] SqlQueryBuilder
{
  public:
    static SqlQueryBuilder From(std::string_view table);

    [[nodiscard]] SqlQueryBuilder& Select(std::vector<std::string_view> const& fieldNames);
    [[nodiscard]] SqlQueryBuilder& Select(std::vector<std::string_view> const& fieldNames, std::string_view tableName);

    template <typename... MoreFields>
    [[nodiscard]] SqlQueryBuilder& Select(std::string_view const& firstField, MoreFields&&... moreFields);

    [[nodiscard]] SqlQueryBuilder& Where(std::string_view sqlConditionExpression);

    template <typename ColumnName, typename T>
    [[nodiscard]] SqlQueryBuilder& Where(ColumnName const& columnName, T const& value);

    [[nodiscard]] SqlQueryBuilder& OrderBy(std::string_view columnName,
                                           SqlResultOrdering ordering = SqlResultOrdering::ASCENDING);

    [[nodiscard]] SqlQueryBuilder& GroupBy(std::string_view columnName);

    [[nodiscard]] SqlQueryBuilder& InnerJoin(std::string_view joinTable,
                                             std::string_view joinColumnName,
                                             SqlQualifiedTableColumnName onComparisonColumn);

    [[nodiscard]] SqlQueryBuilder& InnerJoin(std::string_view joinTable,
                                             std::string_view joinColumnName,
                                             std::string_view onMainTableColumn);

    // final methods

    SqlComposedQuery Count();
    SqlComposedQuery All();
    SqlComposedQuery First();
    SqlComposedQuery Range(std::size_t offset, std::size_t limit);

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

    m_query.condition += " = ?";

    if constexpr (std::is_same_v<T, SqlQueryWildcard>)
        m_query.inputBindings.emplace_back(std::monostate());
    else
        m_query.inputBindings.emplace_back(value);

    return *this;
}

// }}}
