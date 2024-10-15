#include "SqlComposedQuery.hpp"
#include "SqlQueryFormatter.hpp"

#include <sstream>

// {{{ SqlQueryBuilder impl

SqlQueryBuilder SqlQueryBuilder::From(std::string_view table)
{
    return SqlQueryBuilder(table);
}

SqlQueryBuilder::SqlQueryBuilder(std::string_view table)
{
    m_query.table = table;
}

SqlQueryBuilder& SqlQueryBuilder::Select(std::vector<std::string_view> const& fieldNames)
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

SqlQueryBuilder& SqlQueryBuilder::Select(std::vector<std::string_view> const& fieldNames, std::string_view tableName)
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

SqlQueryBuilder& SqlQueryBuilder::InnerJoin(std::string_view joinTable,
                                            std::string_view joinColumnName,
                                            SqlQualifiedTableColumnName onOtherColumn)
{
    m_query.tableJoins += std::format("\n   "
                                      R"(INNER JOIN "{0}" ON "{0}"."{1}" = "{2}"."{3}")",
                                      joinTable,
                                      joinColumnName,
                                      onOtherColumn.tableName,
                                      onOtherColumn.columnName);
    return *this;
}

SqlQueryBuilder& SqlQueryBuilder::InnerJoin(std::string_view joinTable,
                                            std::string_view joinColumnName,
                                            std::string_view onMainTableColumn)
{
    return InnerJoin(joinTable,
                     joinColumnName,
                     SqlQualifiedTableColumnName { .tableName = m_query.table, .columnName = onMainTableColumn });
}

SqlQueryBuilder& SqlQueryBuilder::Where(std::string_view sqlConditionExpression)
{
    if (m_query.condition.empty())
        m_query.condition += " WHERE ";
    else
        m_query.condition += " AND ";

    m_query.condition += "(";
    m_query.condition += std::string(sqlConditionExpression);
    m_query.condition += ")";

    return *this;
}

SqlQueryBuilder& SqlQueryBuilder::OrderBy(std::string_view columnName, SqlResultOrdering ordering)
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

SqlQueryBuilder& SqlQueryBuilder::GroupBy(std::string_view columnName)
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

SqlComposedQuery SqlQueryBuilder::Count()
{
    m_query.type = SqlQueryType::SELECT_COUNT;

    return std::move(m_query);
}

SqlComposedQuery SqlQueryBuilder::All()
{
    m_query.type = SqlQueryType::SELECT_ALL;

    return std::move(m_query);
}

SqlComposedQuery SqlQueryBuilder::First(size_t count)
{
    m_query.type = SqlQueryType::SELECT_FIRST;
    m_query.limit = count;

    return std::move(m_query);
}

SqlComposedQuery SqlQueryBuilder::Range(std::size_t offset, std::size_t limit)
{
    m_query.type = SqlQueryType::SELECT_RANGE;
    m_query.offset = offset;
    m_query.limit = limit;

    return std::move(m_query);
}

SqlComposedQuery SqlQueryBuilder::Delete()
{
    m_query.type = SqlQueryType::DELETE_;

    return std::move(m_query);
}

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
            return formatter.SelectAll(fields, table, tableJoins, *finalCondition, orderBy, groupBy);
        case SqlQueryType::SELECT_FIRST:
            return formatter.SelectFirst(fields, table, tableJoins, *finalCondition, orderBy, limit);
        case SqlQueryType::SELECT_RANGE:
            return formatter.SelectRange(fields, table, tableJoins, *finalCondition, orderBy, groupBy, offset, limit);
        case SqlQueryType::SELECT_COUNT:
            return formatter.SelectCount(table, tableJoins, *finalCondition);
        case SqlQueryType::DELETE_:
            return formatter.Delete(table, tableJoins, *finalCondition);
    }
    return "";
}

// }}}
