#include "SqlStatement.hpp"

SqlStatement::SqlStatement() noexcept:
    m_ownedConnection { SqlConnection() },
    m_connection { &m_ownedConnection.value() },
    m_lastError { m_connection->LastError() }
{
    if (m_lastError == SqlError::SUCCESS)
        UpdateLastError(SQLAllocHandle(SQL_HANDLE_STMT, m_connection->NativeHandle(), &m_hStmt));
}

// Construct a new SqlStatement object, using the given connection.
SqlStatement::SqlStatement(SqlConnection& relatedConnection) noexcept:
    m_connection { &relatedConnection }
{
    UpdateLastError(SQLAllocHandle(SQL_HANDLE_STMT, m_connection->NativeHandle(), &m_hStmt));
}

SqlStatement::~SqlStatement() noexcept
{
    SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
}

SqlResult<void> SqlStatement::Prepare(std::string_view query) noexcept
{
    SqlLogger::GetLogger().OnPrepare(query);

    m_postExecuteCallbacks.clear();
    m_postProcessOutputColumnCallbacks.clear();

    // Closes the cursor if it is open
    return UpdateLastError(SQLFreeStmt(m_hStmt, SQL_CLOSE))
        .and_then([&] {
            // Prepares the statement
            return UpdateLastError(SQLPrepareA(m_hStmt, (SQLCHAR*) query.data(), (SQLINTEGER) query.size()));
        })
        .and_then([&] { return UpdateLastError(SQLNumParams(m_hStmt, &m_expectedParameterCount)); })
        .and_then([&]() -> SqlResult<void> {
            m_indicators.resize(m_expectedParameterCount + 1);
            return {};
        });
}

SqlResult<void> SqlStatement::ExecuteDirect(const std::string_view& query, std::source_location location) noexcept
{
    if (query.empty())
        return {};

    SqlLogger::GetLogger().OnExecuteDirect(query);

    return UpdateLastError(SQLFreeStmt(m_hStmt, SQL_CLOSE), location).and_then([&] {
        return UpdateLastError(SQLExecDirectA(m_hStmt, (SQLCHAR*) query.data(), (SQLINTEGER) query.size()), location);
    });
}

// Retrieves the number of rows affected by the last query.
SqlResult<size_t> SqlStatement::NumRowsAffected() const noexcept
{
    SQLLEN numRowsAffected {};
    return UpdateLastError(SQLRowCount(m_hStmt, &numRowsAffected)).transform([&] { return numRowsAffected; });
}

// Retrieves the number of columns affected by the last query.
SqlResult<size_t> SqlStatement::NumColumnsAffected() const noexcept
{
    SQLSMALLINT numColumns {};
    return UpdateLastError(SQLNumResultCols(m_hStmt, &numColumns)).transform([&] { return numColumns; });
}

// Retrieves the last insert ID of the last query's primary key.
SqlResult<size_t> SqlStatement::LastInsertId() noexcept
{
    switch (m_connection->ServerType())
    {
        case SqlServerType::MICROSOFT_SQL:
            return ExecuteDirect("SELECT @@IDENTITY;").and_then([&] { return FetchRow(); }).and_then([&] {
                return GetColumn<size_t>(1);
            });
        case SqlServerType::POSTGRESQL:
            return ExecuteDirect("SELECT lastval();").and_then([&] { return FetchRow(); }).and_then([&] {
                return GetColumn<size_t>(1);
            });
        case SqlServerType::ORACLE:
            return ExecuteDirect("SELECT LAST_INSERT_ID() FROM DUAL;")
                .and_then([&] { return FetchRow(); })
                .and_then([&] { return GetColumn<size_t>(1); });
        case SqlServerType::SQLITE:
            return ExecuteDirect("SELECT last_insert_rowid();").and_then([&] { return FetchRow(); }).and_then([&] {
                return GetColumn<size_t>(1);
            });
        case SqlServerType::MYSQL:
            return ExecuteDirect("SELECT LAST_INSERT_ID();").and_then([&] { return FetchRow(); }).and_then([&] {
                return GetColumn<size_t>(1);
            });
        case SqlServerType::UNKNOWN:
            return 0;
    }
    return 0;
}

// Fetches the next row of the result set.
SqlResult<void> SqlStatement::FetchRow() noexcept
{
    return UpdateLastError(SQLFetch(m_hStmt)).and_then([&]() -> SqlResult<void> {
        // post-process the output columns, if needed
        for (auto const& postProcess: m_postProcessOutputColumnCallbacks)
            postProcess();
        m_postProcessOutputColumnCallbacks.clear();
        return {};
    });
}

SqlResult<void> SqlStatement::UpdateLastError(SQLRETURN error, std::source_location location) const noexcept
{
    return detail::UpdateSqlError(&m_lastError, error).or_else([&](auto&&) -> SqlResult<void> {
        if (m_lastError != SqlError::NODATA)
            SqlLogger::GetLogger().OnError(m_lastError, SqlErrorInfo::fromStatementHandle(m_hStmt), location);
        return std::unexpected { m_lastError };
    });
}
