#pragma once

#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"

#include <string>
#include <string_view>

struct SqlQualifiedTableColumnName;

// API to format SQL queries for different SQL dialects.
class SqlQueryFormatter
{
  public:
    virtual ~SqlQueryFormatter() = default;

    [[nodiscard]] virtual std::string BooleanWhereClause(SqlQualifiedTableColumnName const& column,
                                                         std::string_view op,
                                                         bool literalValue) const = 0;

    [[nodiscard]] virtual std::string SelectAll(bool distinct,
                                                std::string const& fields,
                                                std::string const& fromTable,
                                                std::string const& tableJoins,
                                                std::string const& whereCondition,
                                                std::string const& orderBy,
                                                std::string const& groupBy) const = 0;

    [[nodiscard]] virtual std::string SelectFirst(bool distinct,
                                                  std::string const& fields,
                                                  std::string const& fromTable,
                                                  std::string const& tableJoins,
                                                  std::string const& whereCondition,
                                                  std::string const& orderBy,
                                                  size_t count) const = 0;

    [[nodiscard]] virtual std::string SelectRange(bool distinct,
                                                  std::string const& fields,
                                                  std::string const& fromTable,
                                                  std::string const& tableJoins,
                                                  std::string const& whereCondition,
                                                  std::string const& orderBy,
                                                  std::string const& groupBy,
                                                  std::size_t offset,
                                                  std::size_t limit) const = 0;

    [[nodiscard]] virtual std::string SelectCount(bool distinct,
                                                  std::string const& fromTable,
                                                  std::string const& tableJoins,
                                                  std::string const& whereCondition) const = 0;

    [[nodiscard]] virtual std::string Delete(std::string const& fromTable,
                                             std::string const& tableJoins,
                                             std::string const& whereCondition) const = 0;

    static SqlQueryFormatter const& Sqlite();
    static SqlQueryFormatter const& SqlServer();
    static SqlQueryFormatter const& PostgrSQL();
    static SqlQueryFormatter const& OracleSQL();

    static SqlQueryFormatter const* Get(SqlServerType serverType) noexcept;
};
