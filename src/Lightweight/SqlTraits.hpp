// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Api.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <functional>
#include <string_view>

// Represents the type of SQL server, used to determine the correct SQL syntax, if needed.
enum class SqlServerType : uint8_t
{
    UNKNOWN,
    MICROSOFT_SQL,
    POSTGRESQL,
    ORACLE,
    SQLITE,
    MYSQL,
};

enum class SqlColumnType : uint8_t
{
    UNKNOWN,
    CHAR,
    STRING,
    TEXT,
    BOOLEAN,
    INTEGER,
    REAL,
    BLOB,
    DATE,
    TIME,
    DATETIME,
};

namespace detail
{

constexpr std::string_view DefaultColumnTypeName(SqlColumnType value) noexcept
{
    switch (value)
    {
        case SqlColumnType::CHAR:
            return "CHAR";
        case SqlColumnType::STRING:
            return "VARCHAR(255)"; // FIXME: This is a guess. Define and use column width somewhere
        case SqlColumnType::TEXT:
            return "TEXT";
        case SqlColumnType::BOOLEAN:
            return "BOOL";
        case SqlColumnType::INTEGER:
            return "INTEGER";
        case SqlColumnType::REAL:
            return "REAL";
        case SqlColumnType::BLOB:
            return "BLOB";
        case SqlColumnType::DATE:
            return "DATE";
        case SqlColumnType::TIME:
            return "TIME";
        case SqlColumnType::DATETIME:
            return "DATETIME";
        case SqlColumnType::UNKNOWN:
            break;
    }
    return "UNKNOWN";
}

constexpr std::string_view MSSqlColumnTypeName(SqlColumnType value) noexcept
{
    switch (value)
    {
        case SqlColumnType::TEXT:
            return "VARCHAR(MAX)";
        case SqlColumnType::BOOLEAN:
            return "BIT";
        default:
            return DefaultColumnTypeName(value);
    }
}

} // namespace detail

struct SqlTraits
{
    std::string_view LastInsertIdQuery;
    std::string_view PrimaryKeyAutoIncrement;
    std::string_view CurrentTimestampExpr;
    std::string_view EnforceForeignKeyConstraint;
    size_t MaxStatementLength {};
    std::function<std::string_view(SqlColumnType)> ColumnTypeName;
};

namespace detail
{

inline SqlTraits const MicrosoftSqlTraits {
    .LastInsertIdQuery = "SELECT @@IDENTITY;",
    .PrimaryKeyAutoIncrement = "INT IDENTITY(1,1) PRIMARY KEY",
    .CurrentTimestampExpr = "GETDATE()",
    .EnforceForeignKeyConstraint = "",
    .ColumnTypeName = detail::MSSqlColumnTypeName,
};

inline SqlTraits const PostgresSqlTraits {
    .LastInsertIdQuery = "SELECT LASTVAL()",
    .PrimaryKeyAutoIncrement = "SERIAL PRIMARY KEY",
    .CurrentTimestampExpr = "CURRENT_TIMESTAMP",
    .EnforceForeignKeyConstraint = "",
    .ColumnTypeName = [](SqlColumnType value) -> std::string_view {
        switch (value)
        {
            case SqlColumnType::DATETIME:
                return "TIMESTAMP";
            default:
                return detail::DefaultColumnTypeName(value);
        }
    },
};

inline SqlTraits const OracleSqlTraits {
    .LastInsertIdQuery = "SELECT LAST_INSERT_ID() FROM DUAL",
    .PrimaryKeyAutoIncrement = "NUMBER GENERATED BY DEFAULT ON NULL AS IDENTITY PRIMARY KEY",
    .CurrentTimestampExpr = "SYSTIMESTAMP",
    .EnforceForeignKeyConstraint = "",
    .ColumnTypeName = detail::DefaultColumnTypeName,
};

inline SqlTraits const SQLiteTraits {
    .LastInsertIdQuery = "SELECT LAST_INSERT_ROWID()",
    .PrimaryKeyAutoIncrement = "INTEGER PRIMARY KEY AUTOINCREMENT",
    .CurrentTimestampExpr = "CURRENT_TIMESTAMP",
    .EnforceForeignKeyConstraint = "PRAGMA foreign_keys = ON",
    .ColumnTypeName = detail::DefaultColumnTypeName,
};

inline SqlTraits const MySQLTraits {
    .LastInsertIdQuery = "SELECT LAST_INSERT_ID()",
    .PrimaryKeyAutoIncrement = "INT AUTO_INCREMENT PRIMARY KEY",
    .CurrentTimestampExpr = "NOW()",
    .EnforceForeignKeyConstraint = "",
    .ColumnTypeName = detail::DefaultColumnTypeName,
};

inline SqlTraits const UnknownSqlTraits {
    .LastInsertIdQuery = "",
    .PrimaryKeyAutoIncrement = "",
    .CurrentTimestampExpr = "",
    .EnforceForeignKeyConstraint = "",
    .ColumnTypeName = detail::DefaultColumnTypeName,
};

} // namespace detail

inline SqlTraits const& GetSqlTraits(SqlServerType serverType) noexcept
{
    auto static const sqlTraits = std::array {
        &detail::UnknownSqlTraits, &detail::MicrosoftSqlTraits, &detail::PostgresSqlTraits,
        &detail::OracleSqlTraits,  &detail::SQLiteTraits,
    };

    return *sqlTraits[static_cast<size_t>(serverType)];
}

template <>
struct LIGHTWEIGHT_API std::formatter<SqlServerType>: std::formatter<std::string_view>
{
    auto format(SqlServerType type, format_context& ctx) const -> format_context::iterator
    {
        string_view name;
        switch (type)
        {
            case SqlServerType::MICROSOFT_SQL:
                name = "Microsoft SQL Server";
                break;
            case SqlServerType::POSTGRESQL:
                name = "PostgreSQL";
                break;
            case SqlServerType::ORACLE:
                name = "Oracle";
                break;
            case SqlServerType::SQLITE:
                name = "SQLite";
                break;
            case SqlServerType::MYSQL:
                name = "MySQL";
                break;
            case SqlServerType::UNKNOWN:
                name = "Unknown";
                break;
        }
        return std::formatter<string_view>::format(name, ctx);
    }
};
