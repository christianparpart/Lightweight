#pragma once

#include "../SqlConnection.hpp"
#include "./Detail.hpp"
#include "./Record.hpp"

#include <string>

namespace Model
{

template <typename... Models>
std::string CreateSqlTablesString(SqlServerType serverType)
{
    detail::StringBuilder result;
    result << ((Models::CreateTableString(serverType) << "\n") << ...);
    return *result;
}

template <typename... Models>
SqlResult<void> CreateSqlTables()
{
    SqlResult<void> result;
    ((result = Models::CreateTable()) && ...);
    return result;
}

} // namespace Model
