// SPDX-License-Identifier: Apache-2.0

#include "SqlConnection.hpp"
#include "SqlTransaction.hpp"

SqlTransaction::SqlTransaction(SqlConnection& connection, SqlTransactionMode defaultMode) noexcept:
    m_hDbc { connection.NativeHandle() },
    m_defaultMode { defaultMode }
{
    SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
}

SqlTransaction::~SqlTransaction() noexcept
{
    switch (m_defaultMode)
    {
        case SqlTransactionMode::NONE:
            break;
        case SqlTransactionMode::COMMIT:
            Commit();
            break;
        case SqlTransactionMode::ROLLBACK:
            Rollback();
            break;
    }
}

bool SqlTransaction::Rollback() noexcept
{
    SQLRETURN sqlReturn = SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_ROLLBACK);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
    {
        SqlLogger::GetLogger().OnError(SqlErrorInfo::fromConnectionHandle(m_hDbc));
        return false;
    }

    sqlReturn = SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
    {
        SqlLogger::GetLogger().OnError(SqlErrorInfo::fromConnectionHandle(m_hDbc));
        return false;
    }

    m_defaultMode = SqlTransactionMode::NONE;
    return true;
}

// Commit the transaction
bool SqlTransaction::Commit() noexcept
{
    SQLRETURN sqlReturn = SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_COMMIT);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
    {
        SqlLogger::GetLogger().OnError(SqlErrorInfo::fromConnectionHandle(m_hDbc));
        return false;
    }

    sqlReturn = SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
    {
        SqlLogger::GetLogger().OnError(SqlErrorInfo::fromConnectionHandle(m_hDbc));
        return false;
    }

    m_defaultMode = SqlTransactionMode::NONE;
    return true;
}
