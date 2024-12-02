// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../Api.hpp"

#include <reflection-cpp/reflection.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

class SqlQueryFormatter;

// clang-format off
namespace SqlColumnTypeDefinitions
{

struct Bool {};
struct Char { size_t size = 1; };
struct Varchar { size_t size {}; };
struct Text { size_t size {}; };
struct Smallint {};
struct Integer {};
struct Bigint {};
struct Real {};
struct Decimal { size_t precision {}; size_t scale {}; };
struct DateTime {};
struct Timestamp {};
struct Date {};
struct Time {};
struct Guid {};

} // namespace SqlColumnTypeDefinitions

using SqlColumnTypeDefinition = std::variant<
    SqlColumnTypeDefinitions::Bigint,
    SqlColumnTypeDefinitions::Bool,
    SqlColumnTypeDefinitions::Char,
    SqlColumnTypeDefinitions::Date,
    SqlColumnTypeDefinitions::DateTime,
    SqlColumnTypeDefinitions::Decimal,
    SqlColumnTypeDefinitions::Guid,
    SqlColumnTypeDefinitions::Integer,
    SqlColumnTypeDefinitions::Real,
    SqlColumnTypeDefinitions::Smallint,
    SqlColumnTypeDefinitions::Text,
    SqlColumnTypeDefinitions::Time,
    SqlColumnTypeDefinitions::Timestamp,
    SqlColumnTypeDefinitions::Varchar
>;
// clang-format on

enum class SqlPrimaryKeyType : uint8_t
{
    NONE,
    MANUAL,
    AUTO_INCREMENT,
    GUID,
};

struct SqlColumnDeclaration
{
    std::string name;
    SqlColumnTypeDefinition type;
    SqlPrimaryKeyType primaryKey { SqlPrimaryKeyType::NONE };
    bool required { false };
    bool unique { false };
    bool index { false };
};

struct SqlCreateTablePlan
{
    std::string_view tableName;
    std::vector<SqlColumnDeclaration> columns;
};

namespace SqlAlterTableCommands
{

struct RenameTable
{
    std::string_view newTableName;
};

struct AddColumn
{
    std::string_view columnName;
    SqlColumnTypeDefinition columnType;
};

struct AddIndex
{
    std::string_view columnName;
    bool unique = false;
};

struct RenameColumn
{
    std::string_view oldColumnName;
    std::string_view newColumnName;
};

struct DropColumn
{
    std::string_view columnName;
};

struct DropIndex
{
    std::string_view columnName;
};

}; // namespace SqlAlterTableCommands

using SqlAlterTableCommand = std::variant<SqlAlterTableCommands::RenameTable,
                                          SqlAlterTableCommands::AddColumn,
                                          SqlAlterTableCommands::AddIndex,
                                          SqlAlterTableCommands::RenameColumn,
                                          SqlAlterTableCommands::DropColumn,
                                          SqlAlterTableCommands::DropIndex>;

struct SqlAlterTablePlan
{
    std::string_view tableName;
    std::vector<SqlAlterTableCommand> commands;
};

struct SqlDropTablePlan
{
    std::string_view tableName;
};

// clang-format off
using SqlMigrationPlanElement = std::variant<
    SqlCreateTablePlan,
    SqlAlterTablePlan,
    SqlDropTablePlan
>;
// clang-format on

struct [[nodiscard]] SqlMigrationPlan
{
    SqlQueryFormatter const& formatter;
    std::vector<SqlMigrationPlanElement> steps {};

    [[nodiscard]] LIGHTWEIGHT_API std::string ToSql() const;

    [[nodiscard]] LIGHTWEIGHT_API static std::string ToSql(SqlQueryFormatter const& formatter,
                                                           SqlMigrationPlanElement const& element);
};
