// SPDX-License-Identifier: Apache-2.0

#include "SqlQuery.hpp"

SqlQueryBuilder& SqlQueryBuilder::FromTable(std::string table)
{
    m_table = std::move(table);
    return *this;
}

SqlQueryBuilder& SqlQueryBuilder::FromTableAs(std::string table, std::string alias)
{
    m_table = std::move(table);
    m_tableAlias = std::move(alias);
    return *this;
}

SqlInsertQueryBuilder SqlQueryBuilder::Insert(std::vector<SqlVariant>* boundInputs) noexcept
{
    return SqlInsertQueryBuilder(m_formatter, std::move(m_table), boundInputs);
}

SqlSelectQueryBuilder SqlQueryBuilder::Select() noexcept
{
    return SqlSelectQueryBuilder(m_formatter, std::move(m_table), std::move(m_tableAlias));
}

SqlUpdateQueryBuilder SqlQueryBuilder::Update(std::vector<SqlVariant>* boundInputs) noexcept
{
    return SqlUpdateQueryBuilder { m_formatter, std::move(m_table), std::move(m_tableAlias), boundInputs };
}

SqlDeleteQueryBuilder SqlQueryBuilder::Delete() noexcept
{
    return SqlDeleteQueryBuilder(m_formatter, std::move(m_table), std::move(m_tableAlias));
}
