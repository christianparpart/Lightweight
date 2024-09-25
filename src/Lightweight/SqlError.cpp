#include "SqlError.hpp"

void SqlErrorInfo::RequireStatementSuccess(SQLRETURN result, SQLHSTMT hStmt, std::string_view message)
{
    if (result == SQL_SUCCESS || result == SQL_SUCCESS_WITH_INFO) [[likely]]
        return;

    throw std::runtime_error { std::format("{}: {}", message, fromStatementHandle(hStmt)) };
}
