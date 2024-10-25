// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "Api.hpp"
#include "SqlTraits.hpp"

#include <format>
#include <string_view>
#include <tuple>
#include <vector>

namespace SqlSchema
{

namespace detail
{
    constexpr std::string_view rtrim(std::string_view value) noexcept
    {
        while (!value.empty() && (std::isspace(value.back()) || value.back() == '\0'))
            value.remove_suffix(1);
        return value;
    }
} // namespace detail

struct FullyQualifiedTableName
{
    std::string catalog;
    std::string schema;
    std::string table;

    bool operator==(FullyQualifiedTableName const& other) const noexcept
    {
        return catalog == other.catalog && schema == other.schema && table == other.table;
    }

    bool operator!=(FullyQualifiedTableName const& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator<(FullyQualifiedTableName const& other) const noexcept
    {
        return std::tie(catalog, schema, table) < std::tie(other.catalog, other.schema, other.table);
    }
};

struct FullyQualifiedTableColumn
{
    FullyQualifiedTableName table;
    std::string column;

    bool operator==(FullyQualifiedTableColumn const& other) const noexcept
    {
        return table == other.table && column == other.column;
    }

    bool operator!=(FullyQualifiedTableColumn const& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator<(FullyQualifiedTableColumn const& other) const noexcept
    {
        return std::tie(table, column) < std::tie(other.table, other.column);
    }
};

struct FullyQualifiedTableColumnSequence
{
    FullyQualifiedTableName table;
    std::vector<std::string> columns;
};

struct ForeignKeyConstraint
{
    FullyQualifiedTableColumn foreignKey;
    FullyQualifiedTableColumnSequence primaryKey;
};

struct Column
{
    std::string name = {};
    SqlColumnType type = SqlColumnType::UNKNOWN;
    std::string dialectDependantTypeString = {};
    bool isNullable = true;
    bool isUnique = false;
    size_t size = 0;
    unsigned short decimalDigits = 0;
    bool isAutoIncrement = false;
    bool isPrimaryKey = false;
    bool isForeignKey = false;
    std::optional<ForeignKeyConstraint> foreignKeyConstraint {};
    std::string defaultValue = {};
};

class EventHandler
{
  public:
    EventHandler() = default;
    EventHandler(EventHandler&&) = default;
    EventHandler(EventHandler const&) = default;
    EventHandler& operator=(EventHandler&&) = default;
    EventHandler& operator=(EventHandler const&) = default;
    virtual ~EventHandler() = default;

    virtual bool OnTable(std::string_view table) = 0;
    virtual void OnPrimaryKeys(std::string_view table, std::vector<std::string> const& columns) = 0;
    virtual void OnForeignKey(ForeignKeyConstraint const& foreignKeyConstraint) = 0;
    virtual void OnColumn(Column const& column) = 0;
    virtual void OnExternalForeignKey(ForeignKeyConstraint const& foreignKeyConstraint) = 0;
    virtual void OnTableEnd() = 0;
};

LIGHTWEIGHT_API void ReadAllTables(std::string_view database, std::string_view schema, EventHandler& eventHandler);

struct Table
{
    // FullyQualifiedTableName name;
    std::string name;
    std::vector<Column> columns {};
    std::vector<ForeignKeyConstraint> foreignKeys {};
    std::vector<ForeignKeyConstraint> externalForeignKeys {};
    std::vector<std::string> primaryKeys {};
};

using TableList = std::vector<Table>;

LIGHTWEIGHT_API TableList ReadAllTables(std::string_view database, std::string_view schema = {});

} // namespace SqlSchema

template <>
struct LIGHTWEIGHT_API std::formatter<SqlSchema::FullyQualifiedTableName>: std::formatter<std::string>
{
    auto format(SqlSchema::FullyQualifiedTableName const& value, format_context& ctx) const -> format_context::iterator
    {
        string output = std::string(SqlSchema::detail::rtrim(value.schema));
        if (!output.empty())
            output += '.';
        auto const trimmedSchema = SqlSchema::detail::rtrim(value.schema);
        output += trimmedSchema;
        if (!output.empty() && !trimmedSchema.empty())
            output += '.';
        output += SqlSchema::detail::rtrim(value.table);
        return formatter<string>::format(output, ctx);
    }
};

template <>
struct LIGHTWEIGHT_API std::formatter<SqlSchema::FullyQualifiedTableColumn>: std::formatter<std::string>
{
    auto format(SqlSchema::FullyQualifiedTableColumn const& value,
                format_context& ctx) const -> format_context::iterator
    {
        auto const table = std::format("{}", value.table);
        if (table.empty())
            return formatter<string>::format(std::format("{}", value.column), ctx);
        else
            return formatter<string>::format(std::format("{}.{}", value.table, value.column), ctx);
    }
};

template <>
struct LIGHTWEIGHT_API std::formatter<SqlSchema::FullyQualifiedTableColumnSequence>: std::formatter<std::string>
{
    auto format(SqlSchema::FullyQualifiedTableColumnSequence const& value,
                format_context& ctx) const -> format_context::iterator
    {
        auto const resolvedTableName = std::format("{}", value.table);
        string output;

        for (auto const& column: value.columns)
        {
            if (!output.empty())
                output += ", ";
            output += resolvedTableName;
            if (!output.empty() && !resolvedTableName.empty())
                output += '.';
            output += column;
        }

        return formatter<string>::format(output, ctx);
    }
};
