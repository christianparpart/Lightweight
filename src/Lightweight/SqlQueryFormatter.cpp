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
    [[nodiscard]] std::string Insert(std::string const& intoTable,
                                     std::string const& fields,
                                     std::string const& values) const override
    {
        return std::format(R"(INSERT INTO "{}" ({}) VALUES ({}))", intoTable, fields, values);
    }

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
                                          std::string const& fromTableAlias,
                                          std::string const& tableJoins,
                                          std::string const& whereCondition) const override
    {
        if (fromTableAlias.empty())
            return std::format(R"(SELECT{} COUNT(*) FROM "{}"{}{})",
                               distinct ? " DISTINCT" : "",
                               fromTable,
                               tableJoins,
                               whereCondition);
        else
            return std::format(R"(SELECT{} COUNT(*) FROM "{}" AS "{}"{}{})",
                               distinct ? " DISTINCT" : "",
                               fromTable,
                               fromTableAlias,
                               tableJoins,
                               whereCondition);
    }

    [[nodiscard]] std::string SelectAll(bool distinct,
                                        std::string const& fields,
                                        std::string const& fromTable,
                                        std::string const& fromTableAlias,
                                        std::string const& tableJoins,
                                        std::string const& whereCondition,
                                        std::string const& orderBy,
                                        std::string const& groupBy) const override
    {
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT ";
        if (distinct)
            sqlQueryString << "DISTINCT ";
        sqlQueryString << fields;
        sqlQueryString << "\n FROM \"" << fromTable << '"';
        if (!fromTableAlias.empty())
            sqlQueryString << " AS \"" << fromTableAlias << '"';
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << groupBy;
        sqlQueryString << orderBy;

        return sqlQueryString.str();
    }

    [[nodiscard]] std::string SelectFirst(bool distinct,
                                          std::string const& fields,
                                          std::string const& fromTable,
                                          std::string const& fromTableAlias,
                                          std::string const& tableJoins,
                                          std::string const& whereCondition,
                                          std::string const& orderBy,
                                          size_t count) const override
    {
        std::stringstream sqlQueryString;
        sqlQueryString << "SELECT " << fields;
        if (distinct)
            sqlQueryString << " DISTINCT";
        sqlQueryString << "\n FROM \"" << fromTable << "\"";
        if (!fromTableAlias.empty())
            sqlQueryString << " AS \"" << fromTableAlias << "\"";
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << orderBy;
        sqlQueryString << " LIMIT " << count;
        return sqlQueryString.str();
    }

    [[nodiscard]] std::string SelectRange(bool distinct,
                                          std::string const& fields,
                                          std::string const& fromTable,
                                          std::string const& fromTableAlias,
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
        sqlQueryString << "\n FROM \"" << fromTable << "\"";
        if (!fromTableAlias.empty())
            sqlQueryString << " AS \"" << fromTableAlias << "\"";
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << groupBy;
        sqlQueryString << orderBy;
        sqlQueryString << " LIMIT " << limit << " OFFSET " << offset;
        return sqlQueryString.str();
    }

    [[nodiscard]] std::string Update(std::string const& table,
                                     std::string const& tableAlias,
                                     std::string const& setFields,
                                     std::string const& whereCondition) const override
    {
        if (tableAlias.empty())
            return std::format(R"(UPDATE "{}" SET {}{})", table, setFields, whereCondition);
        else
            return std::format(R"(UPDATE "{}" AS "{}" SET {}{})", table, tableAlias, setFields, whereCondition);
    }

    [[nodiscard]] std::string Delete(std::string const& fromTable,
                                     std::string const& fromTableAlias,
                                     std::string const& tableJoins,
                                     std::string const& whereCondition) const override
    {
        if (fromTableAlias.empty())
            return std::format(R"(DELETE FROM "{}"{}{})", fromTable, tableJoins, whereCondition);
        else
            return std::format(
                R"(DELETE FROM "{}" AS "{}"{}{})", fromTable, fromTableAlias, tableJoins, whereCondition);
    }
};

class SqlServerQueryFormatter final: public BasicSqlQueryFormatter
{
  public:
    [[nodiscard]] std::string BooleanWhereClause(SqlQualifiedTableColumnName const& column,
                                                 std::string_view op,
                                                 bool literalValue) const override
    {
        auto const literalValueStr = literalValue ? '1' : '0';
        if (!column.tableName.empty())
            return std::format(R"("{}"."{}" {} {})", column.columnName, column.columnName, op, literalValueStr);
        else
            return std::format(R"("{}" {} {})", column.columnName, op, literalValueStr);
    }

    [[nodiscard]] std::string SelectFirst(bool distinct,
                                          std::string const& fields,
                                          std::string const& fromTable,
                                          std::string const& fromTableAlias,
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
        if (!fromTableAlias.empty())
            sqlQueryString << " AS \"" << fromTableAlias << '"';
        sqlQueryString << tableJoins;
        sqlQueryString << whereCondition;
        sqlQueryString << orderBy;
        ;
        return sqlQueryString.str();
    }

    [[nodiscard]] std::string SelectRange(bool distinct,
                                          std::string const& fields,
                                          std::string const& fromTable,
                                          std::string const& fromTableAlias,
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
        if (!fromTableAlias.empty())
            sqlQueryString << " AS \"" << fromTableAlias << "\"";
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
