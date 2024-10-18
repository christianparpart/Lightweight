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
    [[nodiscard]] std::string BooleanWhereClause(SqlQualifiedTableColumnName const& column,
                                                 std::string_view op,
                                                 bool literalValue) const override
    {
        auto const literalValueStr = literalValue ? "TRUE"sv : "FALSE"sv;
        if (!column.tableName.empty())
            return std::format(R"("{}"."{}" {} {})", column.tableName, column.columnName, op, literalValueStr);
        else
            return std::format(R"("{}" {} {})", column.columnName, op, literalValueStr);
    }

    [[nodiscard]] std::string SelectCount(bool distinct,
                                          std::string const& fromTable,
                                          std::string const& tableJoins,
                                          std::string const& whereCondition) const override
    {
        return std::format(
            "SELECT{} COUNT(*) FROM \"{}\"{}{}", distinct ? " DISTINCT" : "", fromTable, tableJoins, whereCondition);
    }

    [[nodiscard]] std::string SelectAll(bool distinct,
                                        std::string const& fields,
                                        std::string const& fromTable,
                                        std::string const& tableJoins,
                                        std::string const& whereCondition,
                                        std::string const& orderBy,
                                        std::string const& groupBy) const override
    {
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT ";
        if (distinct)
            sqlQueryString << "DISTINCT ";
        sqlQueryString << fields << " FROM \"" << fromTable << '"';
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << groupBy;
        sqlQueryString << orderBy;

        return sqlQueryString.str();
    }

    [[nodiscard]] std::string SelectFirst(bool distinct,
                                          std::string const& fields,
                                          std::string const& fromTable,
                                          std::string const& tableJoins,
                                          std::string const& whereCondition,
                                          std::string const& orderBy,
                                          size_t count) const override
    {
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields;
        if (distinct)
            sqlQueryString << " DISTINCT";
        sqlQueryString << " FROM \"" << fromTable << "\"";
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << orderBy;
        sqlQueryString << " LIMIT " << count;
        return sqlQueryString.str();
    }

    [[nodiscard]] std::string SelectRange(bool distinct,
                                          std::string const& fields,
                                          std::string const& fromTable,
                                          std::string const& tableJoins,
                                          std::string const& whereCondition,
                                          std::string const& orderBy,
                                          std::string const& groupBy,
                                          std::size_t offset,
                                          std::size_t limit) const override
    {
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields;
        if (distinct)
            sqlQueryString << " DISTINCT";
        sqlQueryString << " FROM \"" << fromTable << "\"";
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << groupBy;
        sqlQueryString << orderBy;
        sqlQueryString << " LIMIT " << limit << " OFFSET " << offset;
        return sqlQueryString.str();
    }

    [[nodiscard]] std::string Delete(std::string const& fromTable,
                                     std::string const& tableJoins,
                                     std::string const& whereCondition) const override
    {
        return std::format("DELETE FROM \"{}\"{}{}", fromTable, tableJoins, whereCondition);
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

    std::string SelectFirst(bool distinct,
                            std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy,
                            size_t count) const override
    {
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT";
        if (distinct)
            sqlQueryString << " DISTINCT";
        sqlQueryString << " TOP " << count;
        sqlQueryString << ' ' << fields;
        sqlQueryString << " FROM \"" << fromTable << '"';
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << orderBy;
        ;
        return sqlQueryString.str();
    }

    std::string SelectRange(bool distinct,
                            std::string const& fields,
                            std::string const& fromTable,
                            std::string const& tableJoins,
                            std::string const& whereCondition,
                            std::string const& orderBy,
                            std::string const& groupBy,
                            std::size_t offset,
                            std::size_t limit) const override
    {
        assert(!orderBy.empty());
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields;
        if (distinct)
            sqlQueryString << " DISTINCT";
        sqlQueryString << " FROM \"" << fromTable << "\"";
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << groupBy;
        sqlQueryString << orderBy;
        sqlQueryString << " OFFSET " << offset << " ROWS FETCH NEXT " << limit << " ROWS ONLY";
        return sqlQueryString.str();
    }
};

} // namespace

SqlQueryFormatter const& SqlQueryFormatter::Sqlite()
{
    static const BasicSqlQueryFormatter formatter {};
    return formatter;
}

SqlQueryFormatter const& SqlQueryFormatter::SqlServer()
{
    static const SqlServerQueryFormatter formatter {};
    return formatter;
}

SqlQueryFormatter const& SqlQueryFormatter::PostgrSQL()
{
    static const BasicSqlQueryFormatter formatter {};
    return formatter;
}

SqlQueryFormatter const& SqlQueryFormatter::OracleSQL()
{
    return SqlServer(); // So far, Oracle SQL is similar to Microsoft SQL Server.
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
        case SqlServerType::ORACLE:
            return &OracleSQL();
        case SqlServerType::MYSQL: // TODO
        case SqlServerType::UNKNOWN:
            break;
    }
    return nullptr;
}
