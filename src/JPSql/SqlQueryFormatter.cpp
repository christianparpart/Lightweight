#include "SqlQueryFormatter.hpp"

#include <format>
#include <string>

namespace
{

class SqliteQueryFormatter final: public SqlQueryFormatter
{
  public:
    std::string SelectFirst(std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy) const override
    {
        // clang-format off
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields
                       << " FROM \"" << fromTable << "\""
                       << tableJoins
                       << whereCondition
                       << orderBy
                       << " LIMIT 1";
        return sqlQueryString.str();
        // clang-format on
    }

    std::string SelectRange(std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy,
                            std::string const& groupBy,
                            std::size_t offset,
                            std::size_t limit) const
    {
        // clang-format off
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields
                       << " FROM \"" << fromTable << "\""
                       << tableJoins
                       << whereCondition
                       << groupBy
                       << orderBy
                       << " LIMIT " << limit << " OFFSET " << offset;
        return sqlQueryString.str();
        // clang-format on
    }
};

class SqlServerQueryFormatter final: public SqlQueryFormatter
{
  public:
    std::string SelectFirst(std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy) const override
    {
        // clang-format off
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT TOP 1 "
                       << fields
                       << " FROM \"" << fromTable << "\""
                       << tableJoins
                       << whereCondition
                       << orderBy;
        return sqlQueryString.str();
        // clang-format on
    }

    std::string SelectRange(std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy,
                            std::string const& groupBy,
                            std::size_t offset,
                            std::size_t limit) const override
    {
        // clang-format off
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields
                       << " FROM \"" << fromTable << "\""
                       << tableJoins
                       << whereCondition
                       << groupBy
                       << orderBy
                       << " OFFSET " << offset << " ROWS FETCH NEXT " << limit << " ROWS ONLY";
        return sqlQueryString.str();
        // clang-format on
    }
};

} // namespace

std::string SqlQueryFormatter::SelectCount(std::string const& fromTable,
                                           std::string const& tableJoins,
                                           std::string const& whereCondition) const
{
    return std::format("SELECT COUNT(*) FROM \"{}\"{}{}", fromTable, tableJoins, whereCondition);
}

std::string SqlQueryFormatter::SelectAll(std::string const& fields,
                                         std::string const& fromTable,
                                         std::string const& tableJoins,
                                         std::string const& whereCondition,
                                         std::string const& orderBy,
                                         std::string const& groupBy) const
{
    auto const delimiter = tableJoins.empty() ? "" : "\n  ";
    // clang-format off
    std::stringstream sqlQueryString;
    sqlQueryString << "SELECT " << fields
                   << delimiter << " FROM \"" << fromTable << "\""
                   << tableJoins
                   << delimiter << whereCondition
                   << delimiter << groupBy
                   << delimiter << orderBy;
    return sqlQueryString.str();
    // clang-format off
}

SqlQueryFormatter const& SqlQueryFormatter::Sqlite()
{
    static SqliteQueryFormatter formatter {};
    return formatter;
}

SqlQueryFormatter const& SqlQueryFormatter::SqlServer()
{
    static SqlServerQueryFormatter formatter {};
    return formatter;
}

SqlQueryFormatter const* SqlQueryFormatter::Get(SqlServerType serverType) noexcept
{
    switch (serverType)
    {
        case SqlServerType::SQLITE:
            return &Sqlite();
        case SqlServerType::MICROSOFT_SQL:
            return &SqlServer();
        case SqlServerType::POSTGRESQL: // TODO
        case SqlServerType::ORACLE: // TODO
        case SqlServerType::MYSQL: // TODO
        case SqlServerType::UNKNOWN:
            break;
    }
    return nullptr;
}
