#pragma once

#include <EzQuApi/SqlConcepts.hpp>

#if defined(_WIN32)
    #include <Windows.h>
#endif

#include <iostream>
#include <string>
#include <string_view>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

enum class SqlServerType
{
    UNKNOWN,
    MICROSOFT_SQL,
    POSTGRESQL,
};

class SqlConnection
{
  public:
    SqlConnection() noexcept
    {
        SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);
        SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
        SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hDbc);
    }

    ~SqlConnection() noexcept
    {
        SQLDisconnect(m_hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
    }

    // Connects to the given database with the given username and password.
    void Connect(std::string_view database, std::string_view username, std::string_view password) noexcept
    {
        // SetLastError(SQLSetConnectAttrA(m_hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0));
        SetLastError(SQLConnectA(m_hDbc,
                                 (SQLCHAR*) database.data(),
                                 (SQLSMALLINT) database.size(),
                                 (SQLCHAR*) username.data(),
                                 (SQLSMALLINT) username.size(),
                                 (SQLCHAR*) password.data(),
                                 (SQLSMALLINT) password.size()));
        if (IsSuccess())
            SetLastError(
                SQLSetConnectAttrA(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER));
    }

    // Tests if the last query was successful.
    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return m_lastError == SQL_SUCCESS || m_lastError == SQL_SUCCESS_WITH_INFO;
    }

    // Disconnects from the database.
    void Close() noexcept { SetLastError(SQLDisconnect(m_hDbc)); }

    // Retrieves the name of the database in use.
    [[nodiscard]] std::string DatabaseName() const noexcept
    {
        SQLCHAR database[1024] {};
        SQLSMALLINT databaseLen {};
        SetLastError(SQLGetInfoA(m_hDbc, SQL_DATABASE_NAME, database, sizeof(database), &databaseLen));
        return std::string((char const*) database, databaseLen);
    }

    [[nodiscard]] SQLHDBC NativeHandle() const noexcept { return m_hDbc; }

    [[nodiscard]] std::string ServerName() const noexcept
    {
        std::string serverName(1024, '\0');
        SQLSMALLINT serverLen {};
        SQLGetInfoA(m_hDbc, SQL_DBMS_NAME, (SQLPOINTER) serverName.data(), (SQLSMALLINT) serverName.size(), &serverLen);
        serverName.resize(serverLen);
        return serverName;
    }

    [[nodiscard]] SqlServerType ServerType() const noexcept
    {
        auto const serverName = ServerName();
        if (serverName.contains("Microsoft SQL Server"))
            return SqlServerType::MICROSOFT_SQL;
        if (serverName.contains("PostgreSQL"))
            return SqlServerType::POSTGRESQL;
        return SqlServerType::UNKNOWN;
    }

    void SetLastError(SQLRETURN error) const noexcept
    {
        m_lastError = error;
        CheckError();
    }

    void CheckError() const noexcept
    {
        if (IsSuccess() || m_lastError == SQL_NO_DATA)
            return;

        auto const message = GetDiagnosticMessage();
        std::cerr << "SqlStatement error: " << message << '\n';
    }

    [[nodiscard]] std::string GetDiagnosticMessage() const
    {
        SQLCHAR sqlState[6] {};
        SQLINTEGER nativeError {};
        SQLCHAR errMsg[1024] {};
        SQLSMALLINT msgLen {};
        SQLGetDiagRecA(SQL_HANDLE_DBC, m_hDbc, 1, sqlState, &nativeError, errMsg, sizeof(errMsg), &msgLen);
        return std::string((char const*) errMsg, msgLen);
    }

    // Tests if the connection is still active.
    [[nodiscard]] bool IsAlive() const noexcept
    {
        SQLUINTEGER state {};
        SetLastError(SQLGetConnectAttrA(m_hDbc, SQL_ATTR_CONNECTION_DEAD, &state, 0, nullptr));
        return state == SQL_CD_FALSE;
    }

  private:
    SQLHENV m_hEnv;
    SQLHDBC m_hDbc;
    mutable SQLRETURN m_lastError { SQL_SUCCESS };
};

inline std::string SqlErrorString(SqlConnection const& connection) noexcept
{
    SQLCHAR sqlState[6] {};
    SQLINTEGER nativeError {};
    std::string messageText(1024, '\0');
    SQLSMALLINT textLen {};
    SQLGetDiagRecA(SQL_HANDLE_DBC,
                   connection.NativeHandle(),
                   1,
                   sqlState,
                   &nativeError,
                   (SQLCHAR*) messageText.data(),
                   (SQLSMALLINT) messageText.size(),
                   &textLen);
    messageText.resize(textLen);
    return std::string((char const*) sqlState) + ": " + std::move(messageText);
}
