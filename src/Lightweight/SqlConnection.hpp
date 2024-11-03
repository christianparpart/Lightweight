// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "Api.hpp"
#include "SqlConnectInfo.hpp"
#include "SqlError.hpp"
#include "SqlLogger.hpp"
#include "SqlTraits.hpp"

#include <atomic>
#include <chrono>
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

class SqlQueryBuilder;
class SqlQueryFormatter;

// @brief Represents a connection to a SQL database.
class LIGHTWEIGHT_API SqlConnection final
{
  public:
    // Constructs a new SQL connection to the default connection.
    //
    // The default connection is set via SetDefaultConnectInfo.
    // In case the default connection is not set, the connection will fail.
    // And in case the connection fails, the last error will be set.
    SqlConnection();

    // Constructs a new SQL connection to the given connect informaton.
    explicit SqlConnection(std::optional<SqlConnectInfo> connectInfo);

    SqlConnection(SqlConnection&& /*other*/) noexcept;
    SqlConnection& operator=(SqlConnection&& /*other*/) noexcept;
    SqlConnection(SqlConnection const& /*other*/) = delete;
    SqlConnection& operator=(SqlConnection const& /*other*/) = delete;

    // Destructs this SQL connection object,
    ~SqlConnection() noexcept;

    // Retrieves the default connection information.
    static SqlConnectInfo const& DefaultConnectInfo() noexcept;

    // Sets the default connection information.
    static void SetDefaultConnectInfo(SqlConnectInfo connectInfo) noexcept;

    static void SetPostConnectedHook(std::function<void(SqlConnection&)> hook);
    static void ResetPostConnectedHook();

    // Retrieves the connection ID.
    //
    // This is a unique identifier for the connection, which is useful for debugging purposes.
    // Note, this ID will not change if the connection is moved nor when it is reused via the connection pool.
    [[nodiscard]] uint64_t ConnectionId() const noexcept
    {
        return m_connectionId;
    }

    // Closes the connection (attempting to put it back into the connect[[ion pool).
    void Close() noexcept;

    // Connects to the given database with the given username and password.
    //
    // @retval true if the connection was successful.
    // @retval false if the connection failed. Use LastError() to retrieve the error information.
    bool Connect(std::string_view datasource, std::string_view username, std::string_view password) noexcept;

    // Connects to the given database with the given ODBC connection string.
    //
    // @retval true if the connection was successful.
    // @retval false if the connection failed. Use LastError() to retrieve the error information.
    bool Connect(std::string connectionString) noexcept;

    // Connects to the given database with the given username and password.
    //
    // @retval true if the connection was successful.
    // @retval false if the connection failed. Use LastError() to retrieve the error information.
    bool Connect(SqlConnectInfo connectInfo) noexcept;

    // Retrieves the last error information with respect to this SQL connection handle.
    [[nodiscard]] SqlErrorInfo LastError() const;

    // Retrieves the name of the database in use.
    [[nodiscard]] std::string DatabaseName() const;

    // Retrieves the name of the user.
    [[nodiscard]] std::string UserName() const;

    // Retrieves the name of the server.
    [[nodiscard]] std::string ServerName() const;

    // Retrieves the reported server version.
    [[nodiscard]] std::string ServerVersion() const;

    // Retrieves the type of the server.
    [[nodiscard]] SqlServerType ServerType() const noexcept;

    // Retrieves a query formatter suitable for the SQL server being connected.
    [[nodiscard]] SqlQueryFormatter const& QueryFormatter() const noexcept;

    // Creates a new query builder for the given table, compatible with the SQL server being connected.
    [[nodiscard]] SqlQueryBuilder Query(std::string_view const& table = {}) const;

    // Creates a new query builder for the given table with an alias, compatible with the SQL server being connected.
    [[nodiscard]] SqlQueryBuilder QueryAs(std::string_view const& table, std::string_view const& tableAlias) const;

    // Retrieves the SQL traits for the server.
    [[nodiscard]] SqlTraits const& Traits() const noexcept
    {
        return GetSqlTraits(ServerType());
    }

    // Tests if a transaction is active.
    [[nodiscard]] bool TransactionActive() const noexcept;

    // Tests if transactions are allowed.
    [[nodiscard]] bool TransactionsAllowed() const noexcept;

    // Tests if the connection is still active.
    [[nodiscard]] bool IsAlive() const noexcept;

    // Retrieves the connection information.
    [[nodiscard]] SqlConnectInfo const& ConnectionInfo() const noexcept;

    // Retrieves the native handle.
    [[nodiscard]] SQLHDBC NativeHandle() const noexcept
    {
        return m_hDbc;
    }

    // Retrieves the last time the connection was used.
    [[nodiscard]] std::chrono::steady_clock::time_point LastUsed() const noexcept;

    // Sets the last time the connection was used.
    void SetLastUsed(std::chrono::steady_clock::time_point lastUsed) noexcept;

    // Checks the result of an SQL operation, and throws an exception if it is not successful.
    void RequireSuccess(SQLRETURN sqlResult,
                        std::source_location sourceLocation = std::source_location::current()) const;

  private:
    void PostConnect();

    // Private data members
    SQLHENV m_hEnv {};
    SQLHDBC m_hDbc {};
    uint64_t m_connectionId;
    SqlServerType m_serverType = SqlServerType::UNKNOWN;
    SqlQueryFormatter const* m_queryFormatter {};

    struct Data;
    Data* m_data {};
};

inline SqlServerType SqlConnection::ServerType() const noexcept
{
    return m_serverType;
}

inline SqlQueryFormatter const& SqlConnection::QueryFormatter() const noexcept
{
    return *m_queryFormatter;
}
