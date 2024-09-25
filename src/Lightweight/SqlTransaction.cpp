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
    if (m_done)
        return;

    switch (m_defaultMode)
    {
        case SqlTransactionMode::COMMIT:
            Commit();
            break;
        case SqlTransactionMode::ROLLBACK:
            Rollback();
            break;
    }
}

SqlResult<void> SqlTransaction::Rollback()
{
    SqlError ec = SqlError::SUCCESS;
    detail::UpdateSqlError(&ec, SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_ROLLBACK));
    detail::UpdateSqlError(
        &ec, SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER));
    m_done = true;
    if (ec != SqlError::SUCCESS)
        return std::unexpected { ec };
    return {};
}

// Commit the transaction
SqlResult<void> SqlTransaction::Commit()
{
    SqlError ec = SqlError::SUCCESS;
    detail::UpdateSqlError(&ec, SQLEndTran(SQL_HANDLE_DBC, m_hDbc, SQL_COMMIT));
    detail::UpdateSqlError(
        &ec, SQLSetConnectAttr(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER));
    m_done = true;
    if (ec != SqlError::SUCCESS)
        return std::unexpected { ec };
    return {};
}
