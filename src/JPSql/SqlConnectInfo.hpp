#pragma once

#include <chrono>
#include <string>
#include <variant>

struct SqlConnectionString
{
    std::string connectionString;

    auto operator<=>(SqlConnectionString const&) const noexcept = default;
};

struct SqlConnectionDataSource
{
    std::string datasource;
    std::string username;
    std::string password;
    std::chrono::seconds timeout { 5 };

    auto operator<=>(SqlConnectionDataSource const&) const noexcept = default;
};

using SqlConnectInfo = std::variant<SqlConnectionDataSource, SqlConnectionString>;
