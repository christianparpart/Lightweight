// SPDX-License-Identifier: Apache-2.0

#include "SqlConnection.hpp"
#include "SqlTransaction.hpp"

SqlTransaction::SqlTransaction(SqlConnection& connection, SqlTransactionMode defaultMode) noexcept:
    m_hDbc { connection.NativeHandle() },
    m_defaultMode { defaultMode }
{
    SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_OFF, SQL_IS_UINTEGER);
}

SqlTransaction::~SqlTransaction()
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

void SqlTransaction::Rollback()
{
    SQLRETURN sqlReturn = SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_ROLLBACK);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
        throw SqlException { SqlErrorInfo::fromConnectionHandle(m_hDbc) };
    ;
    sqlReturn = SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
        throw SqlException { SqlErrorInfo::fromConnectionHandle(m_hDbc) };

    m_defaultMode = SqlTransactionMode::NONE;
}

// Commit the transaction
void SqlTransaction::Commit()
{
    SQLRETURN sqlReturn = SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_COMMIT);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
        throw SqlException { SqlErrorInfo::fromConnectionHandle(m_hDbc) };

    sqlReturn = SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER);
    if (sqlReturn != SQL_SUCCESS && sqlReturn != SQL_SUCCESS_WITH_INFO)
        throw SqlException { SqlErrorInfo::fromConnectionHandle(m_hDbc) };

    m_defaultMode = SqlTransactionMode::NONE;
}
