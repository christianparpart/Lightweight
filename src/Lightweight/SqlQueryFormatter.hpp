// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Api.hpp"
#include "SqlConnection.hpp"
#include "SqlQuery/MigrationPlan.hpp"

#include <string>
#include <string_view>

struct SqlQualifiedTableColumnName;

// API to format SQL queries for different SQL dialects.
class [[nodiscard]] LIGHTWEIGHT_API SqlQueryFormatter
{
  public:
    SqlQueryFormatter() = default;
    SqlQueryFormatter(SqlQueryFormatter&&) = default;
    SqlQueryFormatter(SqlQueryFormatter const&) = default;
    SqlQueryFormatter& operator=(SqlQueryFormatter&&) = default;
    SqlQueryFormatter& operator=(SqlQueryFormatter const&) = default;
    virtual ~SqlQueryFormatter() = default;

    [[nodiscard]] virtual std::string_view BooleanLiteral(bool value) const noexcept = 0;

    [[nodiscard]] virtual std::string StringLiteral(std::string_view value) const noexcept = 0;

    [[nodiscard]] virtual std::string StringLiteral(char value) const noexcept = 0;

    [[nodiscard]] virtual std::string Insert(std::string const& intoTable,
                                             std::string const& fields,
                                             std::string const& values) const = 0;

    [[nodiscard]] virtual std::string QueryLastInsertId(std::string_view tableName) const = 0;

    [[nodiscard]] virtual std::string SelectAll(bool distinct,
                                                std::string const& fields,
                                                std::string const& fromTable,
                                                std::string const& fromTableAlias,
                                                std::string const& tableJoins,
                                                std::string const& whereCondition,
                                                std::string const& orderBy,
                                                std::string const& groupBy) const = 0;

    [[nodiscard]] virtual std::string SelectFirst(bool distinct,
                                                  std::string const& fields,
                                                  std::string const& fromTable,
                                                  std::string const& fromTableAlias,
                                                  std::string const& tableJoins,
                                                  std::string const& whereCondition,
                                                  std::string const& orderBy,
                                                  size_t count) const = 0;

    [[nodiscard]] virtual std::string SelectRange(bool distinct,
                                                  std::string const& fields,
                                                  std::string const& fromTable,
                                                  std::string const& fromTableAlias,
                                                  std::string const& tableJoins,
                                                  std::string const& whereCondition,
                                                  std::string const& orderBy,
                                                  std::string const& groupBy,
                                                  std::size_t offset,
                                                  std::size_t limit) const = 0;

    [[nodiscard]] virtual std::string SelectCount(bool distinct,
                                                  std::string const& fromTable,
                                                  std::string const& fromTableAlias,
                                                  std::string const& tableJoins,
                                                  std::string const& whereCondition) const = 0;

    [[nodiscard]] virtual std::string Update(std::string const& table,
                                             std::string const& tableAlias,
                                             std::string const& setFields,
                                             std::string const& whereCondition) const = 0;

    [[nodiscard]] virtual std::string Delete(std::string const& fromTable,
                                             std::string const& fromTableAlias,
                                             std::string const& tableJoins,
                                             std::string const& whereCondition) const = 0;

    using StringList = std::vector<std::string>;
    [[nodiscard]] virtual std::string ColumnType(SqlColumnTypeDefinition const& type) const = 0;
    [[nodiscard]] virtual StringList CreateTable(std::string_view tableName, std::vector<SqlColumnDeclaration> const& columns) const = 0;
    [[nodiscard]] virtual StringList AlterTable(std::string_view tableName, std::vector<SqlAlterTableCommand> const& commands) const = 0;
    [[nodiscard]] virtual StringList DropTable(std::string_view const& tableName) const = 0;

    static SqlQueryFormatter const& Sqlite();
    static SqlQueryFormatter const& SqlServer();
    static SqlQueryFormatter const& PostgrSQL();
    static SqlQueryFormatter const& OracleSQL();

    static SqlQueryFormatter const* Get(SqlServerType serverType) noexcept;
};
