#include "SqlComposedQuery.hpp"

#include <utility>
#include "SqlQueryFormatter.hpp"

// {{{ SqlQueryBuilder impl

SqlQueryBuilder SqlQueryBuilder::FromTable(std::string_view table)
{
    return SqlQueryBuilder(std::string(table));
}

SqlQueryBuilder SqlQueryBuilder::FromTableAs(std::string_view table, std::string_view alias)
{
    return SqlQueryBuilder(std::string(table), std::string(alias));
}

SqlQueryBuilder::SqlQueryBuilder(std::string table)
{
    m_query.table = std::move(table);
}

SqlQueryBuilder::SqlQueryBuilder(std::string table, std::string alias)
{
    m_query.table = std::move(table);
    m_query.tableAlias = std::move(alias);
}

SqlSelectQueryBuilder SqlQueryBuilder::Select() && noexcept
{
    return SqlSelectQueryBuilder(std::move(m_query));
}

SqlSelectQueryBuilder SqlQueryBuilder::Select(std::vector<std::string_view> const& fieldNames) &&
{
    return SqlSelectQueryBuilder(std::move(m_query)).Select(fieldNames);
}

SqlSelectQueryBuilder SqlQueryBuilder::Select(std::vector<std::string_view> const& fieldNames,
                                              std::string_view tableName) &&
{
    return SqlSelectQueryBuilder(std::move(m_query)).Select(fieldNames, tableName);
}

SqlComposedQuery SqlQueryBuilder::Delete()
{
    m_query.type = SqlQueryType::DELETE_;

    return std::move(m_query);
}

// }}}

// {{{ SqlSelectQueryBuilder impl
SqlSelectQueryBuilder& SqlSelectQueryBuilder::Distinct() noexcept
{
    m_query.distinct = true;
    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::Select(std::vector<std::string_view> const& fieldNames)
{
    for (auto const& fieldName: fieldNames)
    {
        if (!m_query.fields.empty())
            m_query.fields += ", ";

        m_query.fields += '"';
        m_query.fields += fieldName;
        m_query.fields += '"';
    }
    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::Select(std::vector<std::string_view> const& fieldNames,
                                                     std::string_view tableName)
{
    for (auto const& fieldName: fieldNames)
    {
        if (!m_query.fields.empty())
            m_query.fields += ", ";

        m_query.fields += '"';
        m_query.fields += tableName;
        m_query.fields += "\".\"";
        m_query.fields += fieldName;
        m_query.fields += '"';
    }
    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::OrderBy(std::string_view columnName, SqlResultOrdering ordering)
{
    if (m_query.orderBy.empty())
        m_query.orderBy += " ORDER BY ";
    else
        m_query.orderBy += ", ";

    m_query.orderBy += '"';
    m_query.orderBy += columnName;
    m_query.orderBy += '"';

    if (ordering == SqlResultOrdering::DESCENDING)
        m_query.orderBy += " DESC";
    else if (ordering == SqlResultOrdering::ASCENDING)
        m_query.orderBy += " ASC";
    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::GroupBy(std::string_view columnName)
{
    if (m_query.groupBy.empty())
        m_query.groupBy += " GROUP BY ";
    else
        m_query.groupBy += ", ";

    m_query.groupBy += '"';
    m_query.groupBy += columnName;
    m_query.groupBy += '"';

    return *this;
}

SqlComposedQuery SqlSelectQueryBuilder::Count()
{
    m_query.type = SqlQueryType::SELECT_COUNT;

    return std::move(m_query);
}

SqlComposedQuery SqlSelectQueryBuilder::All()
{
    m_query.type = SqlQueryType::SELECT_ALL;

    return std::move(m_query);
}

SqlComposedQuery SqlSelectQueryBuilder::First(size_t count)
{
    m_query.type = SqlQueryType::SELECT_FIRST;
    m_query.limit = count;

    return std::move(m_query);
}

SqlComposedQuery SqlSelectQueryBuilder::Range(std::size_t offset, std::size_t limit)
{
    m_query.type = SqlQueryType::SELECT_RANGE;
    m_query.offset = offset;
    m_query.limit = limit;

    return std::move(m_query);
}

// }}}

std::string SqlComposedQuery::ToSql(SqlQueryFormatter const& formatter) const
{
    std::string finalConditionBuffer;
    std::string const* finalCondition = &condition;

    if (!booleanLiteralConditions.empty())
    {
        finalConditionBuffer = condition;
        finalCondition = &finalConditionBuffer;
        for (auto&& [column, binaryOp, literalValue]: booleanLiteralConditions)
        {
            if (finalConditionBuffer.empty())
                finalConditionBuffer += " WHERE ";
            else
                finalConditionBuffer += " AND ";

            finalConditionBuffer += formatter.BooleanWhereClause(column, binaryOp, literalValue);
        }
    }

    switch (type)
    {
        case SqlQueryType::UNDEFINED:
            break;
        case SqlQueryType::SELECT_ALL:
            return formatter.SelectAll(distinct, fields, table, tableJoins, *finalCondition, orderBy, groupBy);
        case SqlQueryType::SELECT_FIRST:
            return formatter.SelectFirst(distinct, fields, table, tableJoins, *finalCondition, orderBy, limit);
        case SqlQueryType::SELECT_RANGE:
            return formatter.SelectRange(
                distinct, fields, table, tableJoins, *finalCondition, orderBy, groupBy, offset, limit);
        case SqlQueryType::SELECT_COUNT:
            return formatter.SelectCount(distinct, table, tableJoins, *finalCondition);
        case SqlQueryType::DELETE_:
            return formatter.Delete(table, tableJoins, *finalCondition);
    }
    return "";
}
