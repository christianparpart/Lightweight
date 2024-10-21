#pragma once

#include "Core.hpp"

#include <string>
#include <string_view>
#include <vector>

class [[nodiscard]] SqlUpdateQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlUpdateQueryBuilder>
{
  public:
    SqlUpdateQueryBuilder(SqlQueryFormatter const& formatter,
                          std::string table,
                          std::string tableAlias,
                          std::vector<SqlVariant>* inputBindings) noexcept:
        detail::SqlWhereClauseBuilder<SqlUpdateQueryBuilder> {},
        m_formatter { formatter }
    {
        m_searchCondition.tableName = std::move(table);
        m_searchCondition.tableAlias = std::move(tableAlias);
        m_searchCondition.inputBindings = inputBindings;
    }

    SqlSearchCondition& SearchCondition() noexcept
    {
        return m_searchCondition;
    }

    [[nodiscard]] SqlQueryFormatter const& Formatter() const noexcept
    {
        return m_formatter;
    }

    // Adds a single column to the SET clause.
    template <typename ColumnValue>
    SqlUpdateQueryBuilder& Set(std::string_view columnName, ColumnValue const& value);

    // Adds a single column to the SET clause with the value being a string literal.
    template <std::size_t N>
    SqlUpdateQueryBuilder& Set(std::string_view columnName, char const (&value)[N]);

    // Finalizes building the query as UPDATE ... query.
    [[nodiscard]] std::string ToSql() const;

  private:
    SqlQueryFormatter const& m_formatter;
    std::string m_values;
    SqlSearchCondition m_searchCondition;
};

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
    else if constexpr (std::is_same_v<ColumnValue, SqlWildcardType>)
        m_values += '?';
    else if (m_searchCondition.inputBindings)
    {
        m_values += '?';
        m_searchCondition.inputBindings->emplace_back(value);
    }
    else if constexpr (std::is_same_v<ColumnValue, char>)
        m_values += m_formatter.StringLiteral(value);
    else if constexpr (std::is_arithmetic_v<ColumnValue>)
        m_values += std::format("{}", value);
    else if constexpr (!detail::WhereConditionLiteralType<ColumnValue>::needsQuotes)
        m_values += std::format("{}", value);
    else
    {
        m_values += '\'';
        m_values += std::format("{}", value);
        m_values += '\'';
    }

    return *this;
}

template <std::size_t N>
SqlUpdateQueryBuilder& SqlUpdateQueryBuilder::Set(std::string_view columnName, char const (&value)[N])
{
    return Set(columnName, std::string_view { value, N - 1 });
}

inline std::string SqlUpdateQueryBuilder::ToSql() const
{
    return m_formatter.Update(
        m_searchCondition.tableName, m_searchCondition.tableAlias, m_values, m_searchCondition.condition);
}
