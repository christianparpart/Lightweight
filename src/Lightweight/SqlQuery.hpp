// SPDX-License-Identifier: MIT
#pragma once

#include "SqlQuery/Delete.hpp"
#include "SqlQuery/Insert.hpp"
#include "SqlQuery/Select.hpp"
#include "SqlQuery/Update.hpp"

// API Entry point for building SQL queries.
class [[nodiscard]] SqlQueryBuilder final
{
  public:
    // Constructs a new query builder for the given table.
    explicit SqlQueryBuilder(SqlQueryFormatter const& formatter,
                             std::string&& table = {},
                             std::string&& alias = {}) noexcept;

    // Constructs a new query builder for the given table.
    SqlQueryBuilder& FromTable(std::string table);

    // Constructs a new query builder for the given table with an alias.
    SqlQueryBuilder& FromTableAs(std::string table, std::string alias);

    // Initiates INSERT query building
    //
    // @param boundInputs Optional vector to store bound inputs.
    //                    If provided, the inputs will be appended to this vector and can be used
    //                    to bind the values to the query via SqlStatement::ExecuteWithVariants(...)
    SqlInsertQueryBuilder Insert(std::vector<SqlVariant>* boundInputs = nullptr) noexcept;

    // Initiates SELECT query building
    SqlSelectQueryBuilder Select() noexcept;

    // Initiates UPDATE query building
    //
    // @param boundInputs Optional vector to store bound inputs.
    //                    If provided, the inputs will be appended to this vector and can be used
    //                    to bind the values to the query via SqlStatement::ExecuteWithVariants(...)
    SqlUpdateQueryBuilder Update(std::vector<SqlVariant>* boundInputs = nullptr) noexcept;

    // Initiates DELETE query building
    SqlDeleteQueryBuilder Delete() noexcept;

  private:
    SqlQueryFormatter const& m_formatter;
    std::string m_table;
    std::string m_tableAlias;
};

inline SqlQueryBuilder::SqlQueryBuilder(SqlQueryFormatter const& formatter,
                                        std::string&& table,
                                        std::string&& alias) noexcept:
    m_formatter { formatter },
    m_table { std::move(table) },
    m_tableAlias { std::move(alias) }
{
}
