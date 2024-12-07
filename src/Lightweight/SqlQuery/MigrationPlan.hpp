// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../Api.hpp"
#include "../SqlDataBinder.hpp"
#include "../Utils.hpp"

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
struct NChar { size_t size = 1; };
struct Varchar { size_t size {}; };
struct NVarchar { size_t size {}; };
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
    SqlColumnTypeDefinitions::NChar,
    SqlColumnTypeDefinitions::NVarchar,
    SqlColumnTypeDefinitions::Real,
    SqlColumnTypeDefinitions::Smallint,
    SqlColumnTypeDefinitions::Text,
    SqlColumnTypeDefinitions::Time,
    SqlColumnTypeDefinitions::Timestamp,
    SqlColumnTypeDefinitions::Varchar
>;
// clang-format on

namespace detail
{

template <typename T>
struct SqlColumnTypeDefinitionOf
{
    static_assert(AlwaysFalse<T>, "Unsupported type for SQL column definition.");
};

template <>
struct SqlColumnTypeDefinitionOf<bool>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Bool {};
};

template <>
struct SqlColumnTypeDefinitionOf<char>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Char { 1 };
};

template <>
struct SqlColumnTypeDefinitionOf<SqlDate>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Date {};
};

template <>
struct SqlColumnTypeDefinitionOf<SqlDateTime>
{
    static constexpr auto value = SqlColumnTypeDefinitions::DateTime {};
};

template <>
struct SqlColumnTypeDefinitionOf<SqlTime>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Time {};
};

template <size_t Precision, size_t Scale>
struct SqlColumnTypeDefinitionOf<SqlNumeric<Precision, Scale>>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Decimal { .precision = Precision, .scale = Scale };
};

template <>
struct SqlColumnTypeDefinitionOf<SqlGuid>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Guid {};
};

template <typename T>
    requires(detail::OneOf<T, int16_t, uint16_t>)
struct SqlColumnTypeDefinitionOf<T>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Smallint {};
};

template <typename T>
    requires(detail::OneOf<T, int32_t, uint32_t>)
struct SqlColumnTypeDefinitionOf<T>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Integer {};
};

template <typename T>
    requires(detail::OneOf<T, int64_t, uint64_t>)
struct SqlColumnTypeDefinitionOf<T>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Bigint {};
};

template <typename T>
    requires(detail::OneOf<T, float, double>)
struct SqlColumnTypeDefinitionOf<T>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Real {};
};

template <size_t N, typename CharT>
    requires(detail::OneOf<CharT, char>)
struct SqlColumnTypeDefinitionOf<SqlFixedString<N, CharT, SqlFixedStringMode::VARIABLE_SIZE>>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Varchar { N };
};

template <size_t N, typename CharT>
    requires(detail::OneOf<CharT, char16_t, char32_t, wchar_t>)
struct SqlColumnTypeDefinitionOf<SqlFixedString<N, CharT, SqlFixedStringMode::VARIABLE_SIZE>>
{
    static constexpr auto value = SqlColumnTypeDefinitions::NVarchar { N };
};

template <size_t N, typename CharT>
    requires(detail::OneOf<CharT, char>)
struct SqlColumnTypeDefinitionOf<SqlFixedString<N, CharT, SqlFixedStringMode::FIXED_SIZE>>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Char { N };
};

template <size_t N, typename CharT>
    requires(detail::OneOf<CharT, char16_t, char32_t, wchar_t>)
struct SqlColumnTypeDefinitionOf<SqlFixedString<N, CharT, SqlFixedStringMode::FIXED_SIZE>>
{
    static constexpr auto value = SqlColumnTypeDefinitions::NChar { N };
};

template <size_t N, typename CharT>
struct SqlColumnTypeDefinitionOf<SqlFixedString<N, CharT, SqlFixedStringMode::FIXED_SIZE_RIGHT_TRIMMED>>
{
    static constexpr auto value = SqlColumnTypeDefinitions::Char { N };
};

template <size_t N, typename CharT>
    requires(detail::OneOf<CharT, char16_t, char32_t, wchar_t>)
struct SqlColumnTypeDefinitionOf<SqlFixedString<N, CharT, SqlFixedStringMode::FIXED_SIZE_RIGHT_TRIMMED>>
{
    static constexpr auto value = SqlColumnTypeDefinitions::NChar { N };
};

template <typename T>
struct SqlColumnTypeDefinitionOf<std::optional<T>>
{
    static constexpr auto value = SqlColumnTypeDefinitionOf<T>::value;
};

} // namespace detail

template <typename T>
constexpr auto SqlColumnTypeDefinitionOf = detail::SqlColumnTypeDefinitionOf<T>::value;

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
