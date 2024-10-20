#include "SqlComposedQuery.hpp"
#include "SqlQueryFormatter.hpp"

#include <utility>

// {{{ SqlQueryBuilder impl

SqlQueryBuilder SqlQueryBuilder::FromTable(std::string_view table)
{
    return SqlQueryBuilder { std::string(table), {} };
}

SqlQueryBuilder SqlQueryBuilder::FromTableAs(std::string_view table, std::string_view alias)
{
    return SqlQueryBuilder { std::string(table), std::string(alias) };
}

SqlInsertQueryBuilder SqlQueryBuilder::Insert(std::vector<SqlVariant>* boundInputs) && noexcept
{
    return SqlInsertQueryBuilder(std::move(m_table), boundInputs);
}

SqlSelectQueryBuilder SqlQueryBuilder::Select() && noexcept
{
    return SqlSelectQueryBuilder(std::move(m_table), std::move(m_tableAlias));
}

SqlUpdateQueryBuilder SqlQueryBuilder::Update(std::vector<SqlVariant>* boundInputs) && noexcept
{
    return SqlUpdateQueryBuilder(std::move(m_table), std::move(m_tableAlias), boundInputs);
}

SqlDeleteQueryBuilder SqlQueryBuilder::Delete() && noexcept
{
    return SqlDeleteQueryBuilder { std::move(m_table), std::move(m_tableAlias) };
}

// }}}

// {{{ SqlSelectQueryBuilder impl
SqlSelectQueryBuilder& SqlSelectQueryBuilder::Distinct() noexcept
{
    m_query.distinct = true;
    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::Field(std::string_view const& fieldName)
{
    if (!m_query.fields.empty())
        m_query.fields += ", ";

    m_query.fields += '"';
    m_query.fields += fieldName;
    m_query.fields += '"';

    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::Field(SqlQualifiedTableColumnName const& fieldName)
{
    if (!m_query.fields.empty())
        m_query.fields += ", ";

    m_query.fields += '"';
    m_query.fields += fieldName.tableName;
    m_query.fields += "\".\"";
    m_query.fields += fieldName.columnName;
    m_query.fields += '"';

    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::FieldAs(std::string_view const& fieldName, std::string_view const& alias)
{
    if (!m_query.fields.empty())
        m_query.fields += ", ";

    m_query.fields += '"';
    m_query.fields += fieldName;
    m_query.fields += "\" AS \"";
    m_query.fields += alias;
    m_query.fields += '"';

    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::FieldAs(SqlQualifiedTableColumnName const& fieldName,
                                                      std::string_view const& alias)
{
    if (!m_query.fields.empty())
        m_query.fields += ", ";

    m_query.fields += '"';
    m_query.fields += fieldName.tableName;
    m_query.fields += "\".\"";
    m_query.fields += fieldName.columnName;
    m_query.fields += "\" AS \"";
    m_query.fields += alias;
    m_query.fields += '"';

    return *this;
}

SqlSelectQueryBuilder& SqlSelectQueryBuilder::Fields(std::vector<std::string_view> const& fieldNames)
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

SqlSelectQueryBuilder& SqlSelectQueryBuilder::Fields(std::vector<std::string_view> const& fieldNames,
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
        m_query.orderBy += "\n ORDER BY ";
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
        m_query.groupBy += "\n GROUP BY ";
    else
        m_query.groupBy += ", ";

    m_query.groupBy += '"';
    m_query.groupBy += columnName;
    m_query.groupBy += '"';

    return *this;
}

SqlSelectQueryBuilder::ComposedQuery SqlSelectQueryBuilder::Count()
{
    m_query.selectType = SelectType::Count;

    return std::move(m_query);
}

SqlSelectQueryBuilder::ComposedQuery SqlSelectQueryBuilder::All()
{
    m_query.selectType = SelectType::All;

    return std::move(m_query);
}

SqlSelectQueryBuilder::ComposedQuery SqlSelectQueryBuilder::First(size_t count)
{
    m_query.selectType = SelectType::First;
    m_query.limit = count;

    return std::move(m_query);
}

SqlSelectQueryBuilder::ComposedQuery SqlSelectQueryBuilder::Range(std::size_t offset, std::size_t limit)
{
    m_query.selectType = SelectType::Range;
    m_query.offset = offset;
    m_query.limit = limit;

    return std::move(m_query);
}

// }}}

std::string SqlInsertQueryBuilder::ToSql(SqlQueryFormatter const& formatter) const
{
    return formatter.Insert(m_tableName, m_fields, m_values);
}

std::string SqlUpdateQueryBuilder::ToSql(SqlQueryFormatter const& formatter) const
{
    return formatter.Update(
        m_searchCondition.tableName, m_searchCondition.tableAlias, m_values, m_searchCondition.ToSql(formatter));
}

std::string SqlDeleteQueryBuilder::ToSql(SqlQueryFormatter const& formatter) const
{
    // TODO(pr) don't depend on SqlComposedQuery
    return formatter.Delete(m_searchCondition.tableName,
                            m_searchCondition.tableAlias,
                            m_searchCondition.tableJoins,
                            m_searchCondition.ToSql(formatter));
}

std::string SqlSearchCondition::ToSql(SqlQueryFormatter const& formatter) const
{
    if (booleanLiteralConditions.empty())
    {
        return condition;
    }

    std::string finalCondition;
    finalCondition += condition;
    auto const needsSurrondingParentheses = !condition.empty() && !booleanLiteralConditions.empty();

    if (needsSurrondingParentheses)
        finalCondition += " (";

    for (auto&& [column, binaryOp, literalValue]: booleanLiteralConditions)
    {
        if (finalCondition.empty())
            finalCondition += " WHERE ";
        else
            finalCondition += " AND ";

        finalCondition += formatter.BooleanWhereClause(column, binaryOp, literalValue);
    }

    if (needsSurrondingParentheses)
        finalCondition += ")";

    return finalCondition;
}

std::string SqlSelectQueryBuilder::ComposedQuery::ToSql(SqlQueryFormatter const& formatter) const
{
    switch (selectType)
    {
        case SelectType::All:
            return formatter.SelectAll(distinct,
                                       fields,
                                       searchCondition.tableName,
                                       searchCondition.tableAlias,
                                       searchCondition.tableJoins,
                                       searchCondition.ToSql(formatter),
                                       orderBy,
                                       groupBy);
        case SelectType::First:
            return formatter.SelectFirst(distinct,
                                         fields,
                                         searchCondition.tableName,
                                         searchCondition.tableAlias,
                                         searchCondition.tableJoins,
                                         searchCondition.ToSql(formatter),
                                         orderBy,
                                         limit);
        case SelectType::Range:
            return formatter.SelectRange(distinct,
                                         fields,
                                         searchCondition.tableName,
                                         searchCondition.tableAlias,
                                         searchCondition.tableJoins,
                                         searchCondition.ToSql(formatter),
                                         orderBy,
                                         groupBy,
                                         offset,
                                         limit);
        case SelectType::Count:
            return formatter.SelectCount(distinct,
                                         searchCondition.tableName,
                                         searchCondition.tableAlias,
                                         searchCondition.tableJoins,
                                         searchCondition.ToSql(formatter));
        case SelectType::Undefined:
            break;
    }
    return "";
}
