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

struct SqlComposedQuery
{
    SqlQueryType type = SqlQueryType::UNDEFINED;
    std::string fields;
    std::string table;
    std::vector<SqlVariant> inputBindings;
    std::string condition;
    std::string orderBy;
    std::string groupBy;
    size_t limit = std::numeric_limits<size_t>::max();
    size_t offset = 0;

    std::string ToSql(SqlQueryFormatter const& formatter) const;
};

class SqlQueryBuilder
{
  public:
    static SqlQueryBuilder From(std::string_view table);

    template <typename FirstField>
    SqlQueryBuilder& Select(FirstField&& firstField);

    template <typename FirstField, typename... MoreFields>
    SqlQueryBuilder& Select(FirstField&& firstField, MoreFields&&... moreFields);

    SqlQueryBuilder& Where(std::string_view sqlConditionExpression);

    template <typename ColumnName, typename T>
    SqlQueryBuilder& Where(ColumnName const& columnName, T const& value);

    SqlQueryBuilder& OrderBy(std::string_view columnName, SqlResultOrdering ordering = SqlResultOrdering::ASCENDING);
    SqlQueryBuilder& GroupBy(std::string_view columnName);

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

template <typename FirstField>
SqlQueryBuilder& SqlQueryBuilder::Select(FirstField&& firstField)
{
    using namespace std::string_view_literals;

    if (!m_query.fields.empty())
        m_query.fields += ", ";

    std::ostringstream fragment;
    if (firstField == "*"sv)
        fragment << "*";
    else
        fragment << "\"" << std::forward<FirstField>(firstField) << "\"";

    m_query.fields += fragment.str();
    return *this;
}

template <typename FirstField, typename... MoreFields>
SqlQueryBuilder& SqlQueryBuilder::Select(FirstField&& firstField, MoreFields&&... moreFields)
{
    std::ostringstream fragment;

    if (!m_query.fields.empty())
        fragment << ", ";

    fragment << "\"" << std::forward<FirstField>(firstField) << "\"";
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

    m_query.condition += "\"";
    m_query.condition += columnName;
    m_query.condition += "\" ";
    m_query.condition += "=";
    m_query.condition += " ?";

    m_query.inputBindings.emplace_back(value);

    return *this;
}

// }}}
