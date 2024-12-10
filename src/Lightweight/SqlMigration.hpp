// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Api.hpp"
#include "SqlQuery.hpp"

#include <chrono>
#include <functional>
#include <list>

class SqlConnection;

namespace SqlMigration
{

class MigrationBase;

// Interface to be implemented by the user to execute SQL migrations
class SqlMigrationExecutor
{
  public:
    virtual ~SqlMigrationExecutor() = default;

    virtual void OnCreateTable(SqlCreateTablePlan const& createTable) = 0;
    virtual void OnAlterTable(SqlAlterTablePlan const& alterTable) = 0;

    virtual void OnDropTable(std::string_view tableName) = 0;
};

// Main API to use for managing SQL migrations
class MigrationManager
{
  public:
    using MigrationList = std::list<MigrationBase const*>;

    LIGHTWEIGHT_API static MigrationManager& GetInstance()
    {
        static MigrationManager instance;
        return instance;
    }

    LIGHTWEIGHT_API void AddMigration(MigrationBase const* migration);

    LIGHTWEIGHT_API MigrationList const& GetAllMigrations() const noexcept;

    LIGHTWEIGHT_API void RemoveAllMigrations();

    [[nodiscard]] LIGHTWEIGHT_API std::list<MigrationBase const*> GetPending() const noexcept;

    LIGHTWEIGHT_API void ExecuteMigrations(std::function<void(MigrationBase const& /*currentMigration*/,
                                                              size_t /*currentOffset*/,
                                                              size_t /*total*/)> const& feedbackCallback = {});

  private:
    MigrationList _migrations;
};

// Represents a single unique SQL migration
class MigrationBase
{
  public:
    MigrationBase(uint64_t timestamp, std::string_view title):
        _timestamp { timestamp },
        _title { title }
    {
        MigrationManager::GetInstance().AddMigration(this);
    }

    virtual ~MigrationBase() = default;

    virtual void Execute(SqlMigrationQueryBuilder& planer) const = 0;

    [[nodiscard]] uint64_t GetTimestamp() const noexcept
    {
        return _timestamp;
    }

    [[nodiscard]] std::string_view GetTitle() const noexcept
    {
        return _title;
    }

  private:
    uint64_t _timestamp;
    std::string_view _title;
};

class Migration: public MigrationBase
{
  public:
    Migration(uint64_t timestamp, std::string_view title, std::function<void(SqlMigrationQueryBuilder&)> const& plan):
        MigrationBase(timestamp, title),
        _plan { plan }
    {
    }

    void Execute(SqlMigrationQueryBuilder& planer) const override
    {
        _plan(planer);
    }

  private:
    std::function<void(SqlMigrationQueryBuilder&)> _plan;
};

} // namespace SqlMigration

#define LIGHTWEIGHT_CONCATENATE(s1, s2) s1##s2

#define LIGHTWEIGHT_MIGRATION_INSTANCE(timestamp) migration_##timestamp

// Example use:
//
// LIGHTWEIGHT_SQL_MIGRATION(std::chrono::system_clock(...), "Create table 'MyTable'")
// {
//     // ...
// }
#define LIGHTWEIGHT_SQL_MIGRATION(timestamp, description)                        \
    struct Migration_##timestamp: public SqlMigration::MigrationBase             \
    {                                                                            \
        explicit Migration_##timestamp():                                        \
            MigrationBase(timestamp, description)                                \
        {                                                                        \
        }                                                                        \
                                                                                 \
        void Execute(SqlMigrationQueryBuilder& planer) const override;           \
    };                                                                           \
                                                                                 \
    static Migration_##timestamp LIGHTWEIGHT_CONCATENATE(migration_, timestamp); \
                                                                                 \
    void Migration_##timestamp::Execute(SqlMigrationQueryBuilder& plan) const
