#pragma once

#include "SqlDataBinder.hpp"
#include "SqlConnection.hpp"

class SqlQueryFormatter
{
  public:
    virtual ~SqlQueryFormatter() = default;

    virtual std::string SelectAll(std::string const& fields,
                                  std::string const& fromTable,
                                  std::string const& whereCondition,
                                  std::string const& orderBy,
                                  std::string const& groupBy) const;

    virtual std::string SelectFirst(std::string const& fields,
                                    std::string const& fromTable,
                                    std::string const& whereCondition,
                                    std::string const& orderBy) const = 0;

    virtual std::string SelectRange(std::string const& fields,
                                    std::string const& fromTable,
                                    std::string const& whereCondition,
                                    std::string const& orderBy,
                                    std::string const& groupBy,
                                    std::size_t offset,
                                    std::size_t limit) const = 0;

    virtual std::string SelectCount(std::string const& fromTable, std::string const& whereCondition) const;

    static SqlQueryFormatter const& Sqlite();
    static SqlQueryFormatter const& SqlServer();

    static SqlQueryFormatter const* Get(SqlServerType serverType) noexcept;
};
