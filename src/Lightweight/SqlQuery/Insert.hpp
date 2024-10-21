#pragma once

#include "Core.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

class [[nodiscard]] SqlInsertQueryBuilder final
{
  public:
    explicit SqlInsertQueryBuilder(SqlQueryFormatter const& formatter,
                                   std::string tableName,
                                   std::vector<SqlVariant>* boundInputs) noexcept:
        m_formatter { formatter },
        m_tableName { std::move(tableName) },
        m_boundInputs { boundInputs }
    {
    }

    // Adds a single column to the INSERT query.
    template <typename ColumnValue>
    SqlInsertQueryBuilder& Set(std::string_view columnName, ColumnValue const& value);

    // Finalizes building the query as INSERT INTO ... query.
    [[nodiscard]] std::string ToSql() const;

  private:
    SqlQueryFormatter const& m_formatter;
    std::string m_tableName;
    std::string m_fields;
    std::string m_values;
    std::vector<SqlVariant>* m_boundInputs;
};

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
    else if constexpr (std::is_same_v<ColumnValue, char>)
        m_values += m_formatter.StringLiteral(value);
    else if constexpr (std::is_arithmetic_v<ColumnValue>)
        m_values += std::format("{}", value);
    else if constexpr (std::is_same_v<ColumnValue, SqlWildcardType>)
    {
        m_values += '?';
    }
    else if (m_boundInputs)
    {
        m_values += '?';
        m_boundInputs->emplace_back(value);
    }
    else if constexpr (std::is_convertible_v<ColumnValue, std::string>
                       || std::is_convertible_v<ColumnValue, std::string_view>
                       || std::is_convertible_v<ColumnValue, char const*>)
    {
        m_values += m_formatter.StringLiteral(value);
    }
    else
    {
        m_values += m_formatter.StringLiteral(std::format("{}", value));
    }

    return *this;
}

inline std::string SqlInsertQueryBuilder::ToSql() const
{
    return m_formatter.Insert(m_tableName, m_fields, m_values);
}
