#pragma once

#include "SqlQuery/Insert.hpp"
#include "SqlQuery/Select.hpp"
#include "SqlQuery/Update.hpp"
#include "SqlQuery/Delete.hpp"

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
    SqlInsertQueryBuilder Insert(std::vector<SqlVariant>* boundInputs) noexcept;

    // Initiates SELECT query building
    SqlSelectQueryBuilder Select() noexcept;

    // Initiates UPDATE query building
    SqlUpdateQueryBuilder Update(std::vector<SqlVariant>* boundInputs) noexcept;

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
