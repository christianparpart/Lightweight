#pragma once

#include "Core.hpp"

#include <string>

class [[nodiscard]] SqlDeleteQueryBuilder final: public detail::SqlWhereClauseBuilder<SqlDeleteQueryBuilder>
{
  public:
    explicit SqlDeleteQueryBuilder(SqlQueryFormatter const& formatter,
                                   std::string table,
                                   std::string tableAlias) noexcept:
        detail::SqlWhereClauseBuilder<SqlDeleteQueryBuilder> {},
        m_formatter { formatter }
    {
        m_searchCondition.tableName = std::move(table);
        m_searchCondition.tableAlias = std::move(tableAlias);
    }

    SqlSearchCondition& SearchCondition() noexcept
    {
        return m_searchCondition;
    }

    [[nodiscard]] SqlQueryFormatter const& Formatter() const noexcept
    {
        return m_formatter;
    }

    // Finalizes building the query as DELETE FROM ... query.
    [[nodiscard]] std::string ToSql() const;

  private:
    SqlQueryFormatter const& m_formatter;
    SqlSearchCondition m_searchCondition;
};

inline std::string SqlDeleteQueryBuilder::ToSql() const
{
    return m_formatter.Delete(m_searchCondition.tableName,
                              m_searchCondition.tableAlias,
                              m_searchCondition.tableJoins,
                              m_searchCondition.condition);
}
