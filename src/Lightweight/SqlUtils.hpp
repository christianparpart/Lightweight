// SPDX-License-Identifier: MIT

#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "SqlConnection.hpp"
#include "SqlError.hpp"
#include "SqlStatement.hpp"

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

namespace SqlUtils
{

std::vector<std::string> TableNames(std::string_view database, std::string_view schema = {});

std::vector<std::string> ColumnNames(std::string_view tableName, std::string_view schema = {});

} // namespace SqlUtils
