// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "SqlConnection.hpp"
#include "SqlStatement.hpp"

#include <filesystem>

// TODO: move to public API
class SqlScopedTraceLogger
{
    SQLHDBC m_nativeConnection;

  public:
    explicit SqlScopedTraceLogger(SqlStatement& stmt):
        SqlScopedTraceLogger(stmt.Connection().NativeHandle(),
#if defined(_WIN32) || defined(_WIN64)
                             "CONOUT$"
#else
                             "/dev/stdout"
#endif
        )
    {
    }

    explicit SqlScopedTraceLogger(SQLHDBC hDbc, std::filesystem::path const& logFile):
        m_nativeConnection { hDbc }
    {
        SQLSetConnectAttrA(m_nativeConnection, SQL_ATTR_TRACEFILE, (SQLPOINTER) logFile.string().c_str(), SQL_NTS);
        SQLSetConnectAttrA(m_nativeConnection, SQL_ATTR_TRACE, (SQLPOINTER) SQL_OPT_TRACE_ON, SQL_IS_UINTEGER);
    }

    ~SqlScopedTraceLogger()
    {
        SQLSetConnectAttrA(m_nativeConnection, SQL_ATTR_TRACE, (SQLPOINTER) SQL_OPT_TRACE_OFF, SQL_IS_UINTEGER);
    }
};
