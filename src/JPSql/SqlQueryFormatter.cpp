#include "SqlComposedQuery.hpp"
#include "SqlQueryFormatter.hpp"

#include <cassert>
#include <format>

using namespace std::string_view_literals;

namespace
{

class BasicSqlQueryFormatter: public SqlQueryFormatter
{
  public:
    std::string BooleanWhereClause(SqlQualifiedTableColumnName const& column,
                                   std::string_view op,
                                   bool literalValue) const
    {
        auto const literalValueStr = literalValue ? "TRUE"sv : "FALSE"sv;
        if (!column.tableName.empty())
            return std::format(R"("{}"."{}" {} {})", column.tableName, column.columnName, op, literalValueStr);
        else
            return std::format(R"("{}" {} {})", column.columnName, op, literalValueStr);
    }

    std::string SelectCount(std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition) const
    {
        return std::format("SELECT COUNT(*) FROM \"{}\"{}{}", fromTable, tableJoins, whereCondition);
    }

    std::string SelectAll(std::string const& fields,
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
        // clang-format on
    }

    std::string SelectFirst(std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy,
                            size_t count) const override
    {
        // clang-format off
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields
                       << " FROM \"" << fromTable << "\""
                       << tableJoins
                       << whereCondition
                       << orderBy
                       << " LIMIT " << count;
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

class SqlServerQueryFormatter final: public BasicSqlQueryFormatter
{
  public:
    std::string BooleanWhereClause(SqlQualifiedTableColumnName const& column,
                                   std::string_view op,
                                   bool literalValue) const override
    {
        auto const literalValueStr = literalValue ? '1' : '0';
        if (!column.tableName.empty())
            return std::format(R"("{}"."{}" {} {})", column.columnName, column.columnName, op, literalValueStr);
        else
            return std::format(R"("{}" {} {})", column.columnName, op, literalValueStr);
    }

    std::string SelectFirst(std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy,
                            size_t count) const override
    {
        // clang-format off
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT TOP " << count << " "
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
        assert(!orderBy.empty());
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

SqlQueryFormatter const& SqlQueryFormatter::Sqlite()
{
    static BasicSqlQueryFormatter formatter {};
    return formatter;
}

SqlQueryFormatter const& SqlQueryFormatter::SqlServer()
{
    static SqlServerQueryFormatter formatter {};
    return formatter;
}

SqlQueryFormatter const& SqlQueryFormatter::PostgrSQL()
{
    static BasicSqlQueryFormatter formatter {};
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
        case SqlServerType::POSTGRESQL:
            return &PostgrSQL();
        case SqlServerType::ORACLE: // TODO
        case SqlServerType::MYSQL:  // TODO
        case SqlServerType::UNKNOWN:
            break;
    }
    return nullptr;
}
