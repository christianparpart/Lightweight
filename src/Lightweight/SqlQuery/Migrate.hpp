// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"
#include "MigrationPlan.hpp"

#include <reflection-cpp/reflection.hpp>

/// @brief Query builder for building CREATE TABLE queries.
///
/// @see SqlQueryBuilder
/// @ingroup QueryBuilder
class [[nodiscard]] SqlCreateTableQueryBuilder final
{
  public:
    explicit SqlCreateTableQueryBuilder(SqlCreateTablePlan& plan):
        _plan { plan }
    {
    }

    /// Adds a new column to the table.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& Column(SqlColumnDeclaration column);

    /// Creates a new nullable column.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& Column(std::string columnName, SqlColumnTypeDefinition columnType);

    /// Creates a new column that is non-nullable.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& RequiredColumn(std::string columnName,
                                                               SqlColumnTypeDefinition columnType);

    /// Adds the created_at and updated_at columns to the table.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& Timestamps();

    /// Creates a new primary key column.
    /// Primary keys are always required, unique, have an index, and are non-nullable.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& PrimaryKey(std::string columnName, SqlColumnTypeDefinition columnType);

    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& PrimaryKeyWithAutoIncrement(
        std::string columnName, SqlColumnTypeDefinition columnType = SqlColumnTypeDefinitions::Bigint {});

    /// Creates a new nullable foreign key column.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& ForeignKey(std::string columnName,
                                                           SqlColumnTypeDefinition columnType,
                                                           SqlForeignKeyReferenceDefinition foreignKey);

    /// Creates a new non-nullable foreign key column.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& RequiredForeignKey(std::string columnName,
                                                                   SqlColumnTypeDefinition columnType,
                                                                   SqlForeignKeyReferenceDefinition foreignKey);

    /// Enables the UNIQUE constraint on the last declared column.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& Unique();

    /// Enables the INDEX constraint on the last declared column.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& Index();

    /// Enables the UNIQUE and INDEX constraint on the last declared column and makes it an index.
    LIGHTWEIGHT_API SqlCreateTableQueryBuilder& UniqueIndex();

  private:
    SqlCreateTablePlan& _plan;
};

/// @brief Query builder for building ALTER TABLE queries.
///
/// @see SqlQueryBuilder
/// @ingroup QueryBuilder
class [[nodiscard]] SqlAlterTableQueryBuilder final
{
  public:
    explicit SqlAlterTableQueryBuilder(SqlAlterTablePlan& plan):
        _plan { plan }
    {
    }

    /// Renames the table.
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& RenameTo(std::string_view newTableName);

    /// Adds a new column to the table that is non-nullable.
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& AddColumn(std::string_view columnName,
                                                         SqlColumnTypeDefinition columnType);

    /// Adds a new column to the table that is nullable.
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& AddColumnAsNullable(std::string_view columnName,
                                                                   SqlColumnTypeDefinition columnType);

    /// Alters the column to have a new non-nullable type.
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& AlterColumn(std::string_view columnName,
                                                           SqlColumnTypeDefinition columnType);

    /// Alters the column to have a new nullable type.
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& AlterColumnAsNullable(std::string_view columnName,
                                                                     SqlColumnTypeDefinition columnType);

    /// Renames a column.
    /// @param oldColumnName The old column name.
    /// @param newColumnName The new column name.
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& RenameColumn(std::string_view oldColumnName,
                                                            std::string_view newColumnName);

    /// Drops a column from the table.
    /// @param columnName The name of the column to drop.
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& DropColumn(std::string_view columnName);

    /// Add an index to the table for the specified column.
    /// @param columnName The name of the column to index.
    ///
    /// @code
    /// SqlQueryBuilder q;
    /// q.Migration().AlterTable("Table").AddIndex("column");
    /// // Will execute CREATE INDEX "Table_column_index" ON "Table"("column");
    /// @endcode
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& AddIndex(std::string_view columnName);

    /// Add an index to the table for the specified column that is unique.
    /// @param columnName The name of the column to index.
    ///
    /// @code
    /// SqlQueryBuilder q;
    /// q.Migration().AlterTable("Table").AddUniqueIndex("column");
    /// // Will execute CREATE UNIQUE INDEX "Table_column_index" ON "Table"("column");
    /// @endcode
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& AddUniqueIndex(std::string_view columnName);

    /// Drop an index from the table for the specified column.
    /// @param columnName The name of the column to drop the index from.
    ///
    /// @code
    /// SqlQueryBuilder q;
    /// q.Migration().AlterTable("Table").DropIndex("column");
    /// // Will execute DROP INDEX "Table_column_index";
    /// @endcode
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder& DropIndex(std::string_view columnName);

  private:
    SqlAlterTablePlan& _plan;
};

/// @brief Query builder for building SQL migration queries.
/// @ingroup QueryBuilder
class [[nodiscard]] SqlMigrationQueryBuilder final
{
  public:
    explicit SqlMigrationQueryBuilder(SqlQueryFormatter const& formatter):
        _formatter { formatter },
        _migrationPlan { .formatter = formatter }
    {
    }

    LIGHTWEIGHT_API SqlMigrationQueryBuilder& CreateDatabase(std::string_view databaseName);
    LIGHTWEIGHT_API SqlMigrationQueryBuilder& DropDatabase(std::string_view databaseName);

    LIGHTWEIGHT_API SqlCreateTableQueryBuilder CreateTable(std::string_view tableName);
    LIGHTWEIGHT_API SqlAlterTableQueryBuilder AlterTable(std::string_view tableName);
    LIGHTWEIGHT_API SqlMigrationQueryBuilder& DropTable(std::string_view tableName);

    LIGHTWEIGHT_API SqlMigrationQueryBuilder& RawSql(std::string_view sql);
    LIGHTWEIGHT_API SqlMigrationQueryBuilder& Native(std::function<std::string(SqlConnection&)> callback);

    LIGHTWEIGHT_API SqlMigrationQueryBuilder& BeginTransaction();
    LIGHTWEIGHT_API SqlMigrationQueryBuilder& CommitTransaction();

    LIGHTWEIGHT_API SqlMigrationPlan GetPlan();

  private:
    SqlQueryFormatter const& _formatter;
    SqlMigrationPlan _migrationPlan;
};
