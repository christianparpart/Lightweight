// SPDX-License-Identifier: Apache-2.0

#include "SqlConnection.hpp"
#include "SqlMigration.hpp"

#include <ranges>

namespace SqlMigration
{

void MigrationManager::AddMigration(MigrationBase const* migration)
{
    _migrations.emplace_back(migration);
}

MigrationManager::MigrationList const& MigrationManager::GetAllMigrations() const noexcept
{
    return _migrations;
}

void MigrationManager::RemoveAllMigrations()
{
    _migrations.clear();
}

MigrationManager::MigrationList MigrationManager::GetPending() const noexcept
{
    MigrationList pending;
    for (auto const* migration: _migrations)
        pending.push_back(migration); // TODO: filter those that weren't applied yet
    return pending;
}

#if 0 // TODO
void MigrationManager::ExecuteMigrations(
    std::function<void(MigrationBase const& /*currentMigration*/, size_t /*currentOffset*/, size_t /*total*/)> const&
        feedbackCallback)
{
    auto const pendingMigrations = GetPending();

    auto conn = SqlConnection {};

    for (auto&& [index, migration]: pendingMigrations | std::views::enumerate)
    {
        SqlMigrationExecutor migrator(conn);
        if (feedbackCallback)
            feedbackCallback(*migration, index, _migrations.size());
        migration->Execute(migrator);
    }
}
#endif

} // namespace SqlMigration
