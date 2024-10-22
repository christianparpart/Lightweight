// SPDX-License-Identifier: Apache-2.0

#include "SqlError.hpp"

SqlException::SqlException(SqlErrorInfo info):
    std::runtime_error(std::format("{}", info)),
    _info { std::move(info) }
{
}

void SqlErrorInfo::RequireStatementSuccess(SQLRETURN result, SQLHSTMT hStmt, std::string_view message)
{
    if (result == SQL_SUCCESS || result == SQL_SUCCESS_WITH_INFO) [[likely]]
        return;

    throw std::runtime_error { std::format("{}: {}", message, fromStatementHandle(hStmt)) };
}
