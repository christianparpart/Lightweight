// SPDX-License-Identifier: MIT

#include "SqlConnection.hpp"
#include "SqlSchema.hpp"
#include "SqlStatement.hpp"

#include <algorithm>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

namespace SqlSchema
{

using namespace std::string_literals;
using namespace std::string_view_literals;

using KeyPair = std::pair<FullyQualifiedTableName, FullyQualifiedTableColumn>;

bool operator<(KeyPair const& a, KeyPair const& b)
{
    return std::tie(a.first, a.second) < std::tie(b.first, b.second);
}

namespace
{
    SqlColumnType FromNativeDataType(int value)
    {
        switch (value)
        {
            case SQL_UNKNOWN_TYPE:
                return SqlColumnType::UNKNOWN;
            case SQL_CHAR:
            case SQL_WCHAR:
                return SqlColumnType::CHAR;
            case SQL_VARCHAR:
            case SQL_WVARCHAR:
                return SqlColumnType::STRING;
            case SQL_LONGVARCHAR:
            case SQL_WLONGVARCHAR:
                return SqlColumnType::TEXT;
            case SQL_BIT:
                return SqlColumnType::BOOLEAN;
            case SQL_TINYINT:
                return SqlColumnType::INTEGER;
            case SQL_SMALLINT:
                return SqlColumnType::INTEGER;
            case SQL_INTEGER:
                return SqlColumnType::INTEGER;
            case SQL_BIGINT:
                return SqlColumnType::INTEGER;
            case SQL_REAL:
                return SqlColumnType::REAL;
            case SQL_FLOAT:
                return SqlColumnType::REAL;
            case SQL_DOUBLE:
                return SqlColumnType::REAL;
            case SQL_TYPE_DATE:
                return SqlColumnType::DATE;
            case SQL_TYPE_TIME:
                return SqlColumnType::TIME;
            case SQL_TYPE_TIMESTAMP:
                return SqlColumnType::DATETIME;
            default:
                std::println("Unknown SQL type {}", value);
                return SqlColumnType::UNKNOWN;
        }
    }

    std::vector<std::string> AllTables(std::string_view database, std::string_view schema)
    {
        auto const tableType = "TABLE"sv;
        (void) database;
        (void) schema;

        auto stmt = SqlStatement();
        auto sqlResult = SQLTables(stmt.NativeHandle(),
                                   (SQLCHAR*) database.data(),
                                   (SQLSMALLINT) database.size(),
                                   (SQLCHAR*) schema.data(),
                                   (SQLSMALLINT) schema.size(),
                                   nullptr /* tables */,
                                   0 /* tables length */,
                                   (SQLCHAR*) tableType.data(),
                                   (SQLSMALLINT) tableType.size());
        SqlErrorInfo::RequireStatementSuccess(sqlResult, stmt.NativeHandle(), "SQLTables");

        auto result = std::vector<std::string>();
        while (stmt.FetchRow())
            result.emplace_back(stmt.GetColumn<std::string>(3));

        return result;
    }

    std::vector<ForeignKeyConstraint> AllForeignKeys(FullyQualifiedTableName const& primaryKey,
                                                     FullyQualifiedTableName const& foreignKey)
    {
        auto stmt = SqlStatement();
        auto sqlResult = SQLForeignKeys(stmt.NativeHandle(),
                                        (SQLCHAR*) primaryKey.catalog.data(),
                                        (SQLSMALLINT) primaryKey.catalog.size(),
                                        (SQLCHAR*) primaryKey.schema.data(),
                                        (SQLSMALLINT) primaryKey.schema.size(),
                                        (SQLCHAR*) primaryKey.table.data(),
                                        (SQLSMALLINT) primaryKey.table.size(),
                                        (SQLCHAR*) foreignKey.catalog.data(),
                                        (SQLSMALLINT) foreignKey.catalog.size(),
                                        (SQLCHAR*) foreignKey.schema.data(),
                                        (SQLSMALLINT) foreignKey.schema.size(),
                                        (SQLCHAR*) foreignKey.table.data(),
                                        (SQLSMALLINT) foreignKey.table.size());

        if (!SQL_SUCCEEDED(sqlResult))
            throw std::runtime_error(
                std::format("SQLForeignKeys failed: {}", SqlErrorInfo::fromStatementHandle(stmt.NativeHandle())));

        using ColumnList = std::vector<std::string>;
        auto constraints = std::map<KeyPair, ColumnList>();
        while (stmt.FetchRow())
        {
            auto primaryKeyTable = FullyQualifiedTableName {
                .catalog = stmt.GetColumn<std::string>(1),
                .schema = stmt.GetColumn<std::string>(2),
                .table = stmt.GetColumn<std::string>(3),
            };
            auto foreignKeyTable = FullyQualifiedTableColumn {
                .table =
                    FullyQualifiedTableName {
                        .catalog = stmt.GetColumn<std::string>(5),
                        .schema = stmt.GetColumn<std::string>(6),
                        .table = stmt.GetColumn<std::string>(7),
                    },
                .column = stmt.GetColumn<std::string>(8),
            };
            ColumnList& keyColumns = constraints[{ primaryKeyTable, foreignKeyTable }];
            auto const sequenceNumber = stmt.GetColumn<size_t>(9);
            if (sequenceNumber > keyColumns.size())
                keyColumns.resize(sequenceNumber);
            keyColumns[sequenceNumber - 1] = stmt.GetColumn<std::string>(4);
        }

        auto result = std::vector<ForeignKeyConstraint>();
        for (auto const& [keyPair, columns]: constraints)
        {
            result.emplace_back(ForeignKeyConstraint {
                .foreignKey = keyPair.second,
                .primaryKey = {
                    .table = keyPair.first,
                    .columns = columns,
                },
            });
        }
        return result;
    }

    std::vector<ForeignKeyConstraint> AllForeignKeysTo(FullyQualifiedTableName const& table)
    {
        return AllForeignKeys(table, FullyQualifiedTableName {});
    }

    std::vector<ForeignKeyConstraint> AllForeignKeysFrom(FullyQualifiedTableName const& table)
    {
        return AllForeignKeys(FullyQualifiedTableName {}, table);
    }

    std::vector<std::string> AllPrimaryKeys(FullyQualifiedTableName const& table)
    {
        std::vector<std::string> keys;
        std::vector<size_t> sequenceNumbers;

        auto stmt = SqlStatement();

        auto sqlResult = SQLPrimaryKeys(stmt.NativeHandle(),
                                        (SQLCHAR*) table.catalog.data(),
                                        (SQLSMALLINT) table.catalog.size(),
                                        (SQLCHAR*) table.schema.data(),
                                        (SQLSMALLINT) table.schema.size(),
                                        (SQLCHAR*) table.table.data(),
                                        (SQLSMALLINT) table.table.size());
        if (!SQL_SUCCEEDED(sqlResult))
            throw std::runtime_error(
                std::format("SQLPrimaryKeys failed: {}", SqlErrorInfo::fromStatementHandle(stmt.NativeHandle())));

        while (stmt.FetchRow())
        {
            keys.emplace_back(stmt.GetColumn<std::string>(4));
            sequenceNumbers.emplace_back(stmt.GetColumn<size_t>(5));
        }

        std::vector<std::string> sortedKeys;
        sortedKeys.resize(keys.size());
        for (size_t i = 0; i < keys.size(); ++i)
            sortedKeys.at(sequenceNumbers[i] - 1) = keys[i];

        return sortedKeys;
    }

} // namespace

void ReadAllTables(std::string_view database, std::string_view schema, EventHandler& eventHandler)
{
    auto const tableNames = AllTables(database, schema);

    for (auto& tableName: tableNames)
    {
        if (tableName == "sqlite_sequence")
            continue;

        if (!eventHandler.OnTable(tableName))
            continue;

        auto const fullyQualifiedTableName = FullyQualifiedTableName {
            .catalog = std::string(database),
            .schema = std::string(schema),
            .table = std::string(tableName),
        };

        auto const primaryKeys = AllPrimaryKeys(fullyQualifiedTableName);
        eventHandler.OnPrimaryKeys(tableName, primaryKeys);

        auto const foreignKeys = AllForeignKeysFrom(fullyQualifiedTableName);
        auto const incomingForeignKeys = AllForeignKeysTo(fullyQualifiedTableName);

        for (auto const& foreignKey: foreignKeys)
            eventHandler.OnForeignKey(foreignKey);

        for (auto const& foreignKey: incomingForeignKeys)
            eventHandler.OnExternalForeignKey(foreignKey);

        auto columnStmt = SqlStatement();
        auto const sqlResult = SQLColumns(columnStmt.NativeHandle(),
                                          (SQLCHAR*) database.data(),
                                          (SQLSMALLINT) database.size(),
                                          (SQLCHAR*) schema.data(),
                                          (SQLSMALLINT) schema.size(),
                                          (SQLCHAR*) tableName.data(),
                                          (SQLSMALLINT) tableName.size(),
                                          nullptr /* column name */,
                                          0 /* column name length */);
        if (!SQL_SUCCEEDED(sqlResult))
            throw std::runtime_error(
                std::format("SQLColumns failed: {}", SqlErrorInfo::fromStatementHandle(columnStmt.NativeHandle())));

        Column column;

        while (columnStmt.FetchRow())
        {
            column.name = columnStmt.GetColumn<std::string>(4);
            column.type = FromNativeDataType(columnStmt.GetColumn<int>(5));
            column.dialectDependantTypeString = columnStmt.GetColumn<std::string>(6);
            column.size = columnStmt.GetColumn<int>(7);
            // 8 - bufferLength
            column.decimalDigits = columnStmt.GetColumn<uint16_t>(9);
            // 10 - NUM_PREC_RADIX
            column.isNullable = columnStmt.GetColumn<bool>(11);
            // 12 - remarks
            column.defaultValue = columnStmt.GetColumn<std::string>(13);

            // accumulated properties
            column.isPrimaryKey = std::ranges::contains(primaryKeys, column.name);
            // column.isForeignKey = ...;
            column.isForeignKey = std::ranges::any_of(
                foreignKeys, [&column](auto const& fk) { return fk.foreignKey.column == column.name; });
            if (auto const p = std::ranges::find_if(
                    incomingForeignKeys, [&column](auto const& fk) { return fk.foreignKey.column == column.name; });
                p != incomingForeignKeys.end())
            {
                column.foreignKeyConstraint = *p;
            }

            eventHandler.OnColumn(column);
        }

        eventHandler.OnTableEnd();
    }
}

std::string ToLowerCase(std::string_view str)
{
    std::string result(str);
    std::transform(
        result.begin(), result.end(), result.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

TableList ReadAllTables(std::string_view database, std::string_view schema)
{
    TableList tables;
    struct EventHandler: public SqlSchema::EventHandler
    {
        TableList& tables;
        EventHandler(TableList& tables):
            tables(tables)
        {
        }

        bool OnTable(std::string_view table) override
        {
            tables.emplace_back(Table { .name = std::string(table) });
            return true;
        }

        void OnTableEnd() override {}

        void OnColumn(SqlSchema::Column const& column) override
        {
            tables.back().columns.emplace_back(column);
        }

        void OnPrimaryKeys(std::string_view /*table*/, std::vector<std::string> const& columns) override
        {
            tables.back().primaryKeys = columns;
        }

        void OnForeignKey(SqlSchema::ForeignKeyConstraint const& foreignKeyConstraint) override
        {
            tables.back().foreignKeys.emplace_back(foreignKeyConstraint);
        }

        void OnExternalForeignKey(SqlSchema::ForeignKeyConstraint const& foreignKeyConstraint) override
        {
            tables.back().externalForeignKeys.emplace_back(foreignKeyConstraint);
        }
    } eventHandler { tables };
    ReadAllTables(database, schema, eventHandler);

    std::map<std::string, std::string> tableNameCaseMap;
    for (auto const& table: tables)
        tableNameCaseMap[ToLowerCase(table.name)] = table.name;

    // Fixup table names in foreign keys
    // (Because at least Sqlite returns them in lowercase)
    for (auto& table: tables)
    {
        for (auto& key: table.foreignKeys)
        {
            key.primaryKey.table.table = tableNameCaseMap.at(ToLowerCase(key.primaryKey.table.table));
            key.foreignKey.table.table = tableNameCaseMap.at(ToLowerCase(key.foreignKey.table.table));
        }
        for (auto& key: table.externalForeignKeys)
        {
            key.primaryKey.table.table = tableNameCaseMap.at(ToLowerCase(key.primaryKey.table.table));
            key.foreignKey.table.table = tableNameCaseMap.at(ToLowerCase(key.foreignKey.table.table));
        }
    }

    return tables;
}

} // namespace SqlSchema
