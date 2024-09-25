#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "SqlConcepts.hpp"
#include "SqlConnectInfo.hpp"
#include "SqlError.hpp"
#include "SqlLogger.hpp"
#include "SqlTraits.hpp"

#include <atomic>
#include <chrono>
#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

class SqlQueryFormatter;

// @brief Represents a connection to a SQL database.
class SqlConnection final
{
  public:
    // Constructs a new SQL connection to the default connection.
    //
    // The default connection is set via SetDefaultConnectInfo.
    // In case the default connection is not set, the connection will fail.
    // And in case the connection fails, the last error will be set.
    SqlConnection() noexcept;

    // Constructs a new SQL connection to the given connect informaton.
    explicit SqlConnection(SqlConnectInfo const& connectInfo) noexcept;

    SqlConnection(SqlConnection&&) noexcept;
    SqlConnection& operator=(SqlConnection&&) noexcept;
    SqlConnection(SqlConnection const&) = delete;
    SqlConnection& operator=(SqlConnection const&) = delete;

    // Destructs this SQL connection object,
    ~SqlConnection() noexcept;

    // Retrieves the default connection information.
    static SqlConnectInfo const& DefaultConnectInfo() noexcept
    {
        return m_gDefaultConnectInfo.value();
    }

    // Sets the default connection information.
    static void SetDefaultConnectInfo(SqlConnectInfo connectInfo) noexcept
    {
        m_gDefaultConnectInfo = std::move(connectInfo);
    }

    // Sets the maximum number of idle connections in the connection pool.
    static void SetMaxIdleConnections(size_t maxIdleConnections) noexcept;

    // Kills all idle connections in the connection pool.
    static void KillAllIdle();

    static void SetPostConnectedHook(std::function<void(SqlConnection&)> hook);
    static void ResetPostConnectedHook();

    static SqlConnectionStats Stats() noexcept;

    // Retrieves the connection ID.
    //
    // This is a unique identifier for the connection, which is useful for debugging purposes.
    // Note, this ID will not change if the connection is moved nor when it is reused via the connection pool.
    [[nodiscard]] uint64_t ConnectionId() const noexcept
    {
        return m_connectionId;
    }

    // Closes the connection (attempting to put it back into the connection pool).
    void Close() noexcept;

    // Kills the connection.
    void Kill() noexcept;

    // Connects to the given database with the given username and password.
    bool Connect(std::string_view datasource, std::string_view username, std::string_view password) noexcept;

    // Connects to the given database with the given ODBC connection string.
    bool Connect(std::string connectionString) noexcept;

    // Connects to the given database with the given username and password.
    bool Connect(SqlConnectInfo connectInfo) noexcept;

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
    [[nodiscard]] SqlConnectInfo const& ConnectionInfo() const noexcept
    {
        return m_connectInfo;
    }

    // Retrieves the native handle.
    [[nodiscard]] SQLHDBC NativeHandle() const noexcept
    {
        return m_hDbc;
    }

    // Retrieves the last error code.
    [[nodiscard]] SqlError LastError() const noexcept
    {
        return m_lastError;
    }

    // Retrieves the last time the connection was used.
    [[nodiscard]] std::chrono::steady_clock::time_point LastUsed() const noexcept
    {
        return m_lastUsed;
    }

    // Sets the last time the connection was used.
    void SetLastUsed(std::chrono::steady_clock::time_point lastUsed) noexcept
    {
        m_lastUsed = lastUsed;
    }

  private:
    void PostConnect();

    void RequireSuccess(SQLRETURN error, std::source_location sourceLocation = std::source_location::current()) const;

    // Updates the last error code and returns the error code as an SqlResult if the operation failed.
    //
    // We also log here the error message.
    SqlResult<void> UpdateLastError(
        SQLRETURN error, std::source_location sourceLocation = std::source_location::current()) const noexcept;

    // Private data members

    static inline std::optional<SqlConnectInfo> m_gDefaultConnectInfo;
    static inline std::atomic<uint64_t> m_gNextConnectionId { 1 };
    static inline std::function<void(SqlConnection&)> m_gPostConnectedHook {};

    SQLHENV m_hEnv {};
    SQLHDBC m_hDbc {};
    uint64_t m_connectionId { m_gNextConnectionId++ };
    mutable SqlError m_lastError {};
    SqlConnectInfo m_connectInfo;
    std::chrono::steady_clock::time_point m_lastUsed; // Last time the connection was used (mostly interesting for
                                                      // idle connections in the connection pool).
    SqlServerType m_serverType = SqlServerType::UNKNOWN;
    SqlQueryFormatter const* m_queryFormatter {};
};

inline SqlServerType SqlConnection::ServerType() const noexcept
{
    return m_serverType;
}

inline SqlQueryFormatter const& SqlConnection::QueryFormatter() const noexcept
{
    return *m_queryFormatter;
}
