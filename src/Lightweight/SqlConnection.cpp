// SPDX-License-Identifier: Apache-2.0

#include "SqlConnection.hpp"
#include "SqlQuery.hpp"
#include "SqlQueryFormatter.hpp"

#include <list>
#include <mutex>
#include <print>

#include <sql.h>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

static std::list<SqlConnection> g_unusedConnections;

class SqlConnectionPool
{
  public:
    ~SqlConnectionPool()
    {
        KillAllIdleConnections();
    }

    void KillAllIdleConnections()
    {
        auto const _ = std::lock_guard { m_unusedConnectionsMutex };
        for (auto& connection: m_unusedConnections)
            connection.Kill();
        m_unusedConnections.clear();
    }

    SqlConnection AcquireDirect()
    {
        auto const _ = std::lock_guard { m_unusedConnectionsMutex };

        // Close idle connections
        auto const now = std::chrono::steady_clock::now();
        while (!m_unusedConnections.empty() && now - m_unusedConnections.front().LastUsed() > m_connectionTimeout)
        {
            ++m_stats.timedout;
            SqlLogger::GetLogger().OnConnectionIdle(m_unusedConnections.front());
            m_unusedConnections.front().Kill();
            m_unusedConnections.pop_front();
        }

        // Reuse an existing connection
        if (!m_unusedConnections.empty())
        {
            ++m_stats.reused;
            auto connection = std::move(m_unusedConnections.front());
            m_unusedConnections.pop_front();
            SqlLogger::GetLogger().OnConnectionReuse(connection);
            return connection;
        }

        // Create a new connection
        ++m_stats.created;
        auto connection = SqlConnection { SqlConnection::DefaultConnectInfo() };
        return connection;
    }

    void Release(SqlConnection&& connection)
    {
        auto const _ = std::lock_guard { m_unusedConnectionsMutex };
        ++m_stats.released;
        if (m_unusedConnections.size() < m_maxIdleConnections)
        {
            connection.SetLastUsed(std::chrono::steady_clock::now());
            SqlLogger::GetLogger().OnConnectionReuse(connection);
            m_unusedConnections.emplace_back(std::move(connection));
        }
        else
        {
            SqlLogger::GetLogger().OnConnectionIdle(connection);
            connection.Kill();
        }
    }

    void SetMaxIdleConnections(size_t maxIdleConnections) noexcept
    {
        m_maxIdleConnections = maxIdleConnections;
    }

    [[nodiscard]] SqlConnectionStats Stats() const noexcept
    {
        return m_stats;
    }

  private:
    std::list<SqlConnection> m_unusedConnections;
    std::mutex m_unusedConnectionsMutex;
    size_t m_maxIdleConnections = 10;
    std::chrono::seconds m_connectionTimeout = std::chrono::seconds { 120 };
    SqlConnectionStats m_stats;
};

static SqlConnectionPool g_connectionPool;

// =====================================================================================================================

SqlConnection::SqlConnection() noexcept:
    SqlConnection(g_connectionPool.AcquireDirect())
{
}

SqlConnection::SqlConnection(SqlConnectInfo const& connectInfo) noexcept
{
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);
    SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hDbc);
    Connect(connectInfo);
}

SqlConnection::SqlConnection(SqlConnection&& other) noexcept:
    m_hEnv { other.m_hEnv },
    m_hDbc { other.m_hDbc },
    m_connectionId { other.m_connectionId },
    m_connectInfo { std::move(other.m_connectInfo) },
    m_lastUsed { other.m_lastUsed },
    m_serverType { other.m_serverType },
    m_queryFormatter { other.m_queryFormatter }
{
    other.m_hEnv = {};
    other.m_hDbc = {};
}

SqlConnection& SqlConnection::operator=(SqlConnection&& other) noexcept
{
    if (this == &other)
        return *this;

    Close();

    m_hEnv = other.m_hEnv;
    m_hDbc = other.m_hDbc;
    m_connectionId = other.m_connectionId;
    m_connectInfo = std::move(other.m_connectInfo);
    m_lastUsed = other.m_lastUsed;

    other.m_hEnv = {};
    other.m_hDbc = {};

    return *this;
}

SqlConnection::~SqlConnection() noexcept
{
    Close();
}

void SqlConnection::SetMaxIdleConnections(size_t maxIdleConnections) noexcept
{
    g_connectionPool.SetMaxIdleConnections(maxIdleConnections);
}

void SqlConnection::KillAllIdle()
{
    g_connectionPool.KillAllIdleConnections();
}

void SqlConnection::SetPostConnectedHook(std::function<void(SqlConnection&)> hook)
{
    m_gPostConnectedHook = std::move(hook);
}

void SqlConnection::ResetPostConnectedHook()
{
    m_gPostConnectedHook = {};
}

SqlConnectionStats SqlConnection::Stats() noexcept
{
    return g_connectionPool.Stats();
}

bool SqlConnection::Connect(std::string_view datasource, std::string_view username, std::string_view password) noexcept
{
    return Connect(SqlConnectionDataSource {
        .datasource = std::string(datasource), .username = std::string(username), .password = std::string(password) });
}

// Connects to the given database with the given ODBC connection string.
bool SqlConnection::Connect(std::string connectionString) noexcept
{
    return Connect(SqlConnectionString { .value = std::move(connectionString) });
}

void SqlConnection::PostConnect()
{
    auto const mappings = std::array {
        std::pair { "Microsoft SQL Server"sv, SqlServerType::MICROSOFT_SQL },
        std::pair { "PostgreSQL"sv, SqlServerType::POSTGRESQL },
        std::pair { "Oracle"sv, SqlServerType::ORACLE },
        std::pair { "SQLite"sv, SqlServerType::SQLITE },
        std::pair { "MySQL"sv, SqlServerType::MYSQL },
    };

    auto const serverName = ServerName();
    for (auto const& [name, type]: mappings)
    {
        if (serverName.contains(name))
        {
            m_serverType = type;
            break;
        }
    }

    m_queryFormatter = SqlQueryFormatter::Get(m_serverType);
}

// Connects to the given database with the given username and password.
bool SqlConnection::Connect(SqlConnectInfo connectInfo) noexcept
{
    m_connectInfo = std::move(connectInfo);

    if (auto const* info = std::get_if<SqlConnectionDataSource>(&m_connectInfo))
    {
        SQLRETURN sqlReturn = SQLSetConnectAttrA(m_hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER) info->timeout.count(), 0);
        if (!SQL_SUCCEEDED(sqlReturn))
        {
            SqlLogger::GetLogger().OnError(SqlErrorInfo::fromConnectionHandle(m_hDbc));
            return false;
        }

        sqlReturn = SQLConnectA(m_hDbc,
                                (SQLCHAR*) info->datasource.data(),
                                (SQLSMALLINT) info->datasource.size(),
                                (SQLCHAR*) info->username.data(),
                                (SQLSMALLINT) info->username.size(),
                                (SQLCHAR*) info->password.data(),
                                (SQLSMALLINT) info->password.size());
        if (!SQL_SUCCEEDED(sqlReturn))
        {
            SqlLogger::GetLogger().OnError(SqlErrorInfo::fromConnectionHandle(m_hDbc));
            return false;
        }

        sqlReturn = SQLSetConnectAttrA(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER);
        if (!SQL_SUCCEEDED(sqlReturn))
        {
            SqlLogger::GetLogger().OnError(SqlErrorInfo::fromConnectionHandle(m_hDbc));
            return false;
        }

        PostConnect();

        SqlLogger::GetLogger().OnConnectionOpened(*this);

        if (m_gPostConnectedHook)
            m_gPostConnectedHook(*this);
    }

    auto const& connectionString = std::get<SqlConnectionString>(m_connectInfo).value;

    SQLRETURN sqlResult = SQLDriverConnectA(m_hDbc,
                                            (SQLHWND) nullptr,
                                            (SQLCHAR*) connectionString.data(),
                                            (SQLSMALLINT) connectionString.size(),
                                            nullptr,
                                            0,
                                            nullptr,
                                            SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(sqlResult))
        return false;

    sqlResult = SQLSetConnectAttrA(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER);
    if (!SQL_SUCCEEDED(sqlResult))
        return false;

    PostConnect();
    SqlLogger::GetLogger().OnConnectionOpened(*this);

    if (m_gPostConnectedHook)
        m_gPostConnectedHook(*this);

    return true;
}

void SqlConnection::Close() noexcept
{
    if (!m_hDbc)
        return;

    if (m_connectInfo == DefaultConnectInfo())
        g_connectionPool.Release(std::move(*this));
    else
        Kill();
}

void SqlConnection::Kill() noexcept
{
    if (!m_hDbc)
        return;

    SqlLogger::GetLogger().OnConnectionClosed(*this);

    SQLDisconnect(m_hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);

    m_hDbc = {};
    m_hEnv = {};
}

std::string SqlConnection::DatabaseName() const
{
    std::string name(128, '\0');
    SQLSMALLINT nameLen {};
    RequireSuccess(SQLGetInfoA(m_hDbc, SQL_DATABASE_NAME, name.data(), (SQLSMALLINT) name.size(), &nameLen));
    name.resize(nameLen);
    return name;
}

std::string SqlConnection::UserName() const
{
    std::string name(128, '\0');
    SQLSMALLINT nameLen {};
    RequireSuccess(SQLGetInfoA(m_hDbc, SQL_USER_NAME, name.data(), (SQLSMALLINT) name.size(), &nameLen));
    name.resize(nameLen);
    return name;
}

std::string SqlConnection::ServerName() const
{
    std::string name(128, '\0');
    SQLSMALLINT nameLen {};
    RequireSuccess(SQLGetInfoA(m_hDbc, SQL_DBMS_NAME, (SQLPOINTER) name.data(), (SQLSMALLINT) name.size(), &nameLen));
    name.resize(nameLen);
    return name;
}

std::string SqlConnection::ServerVersion() const
{
    std::string text(128, '\0');
    SQLSMALLINT textLen {};
    RequireSuccess(SQLGetInfoA(m_hDbc, SQL_DBMS_VER, (SQLPOINTER) text.data(), (SQLSMALLINT) text.size(), &textLen));
    text.resize(textLen);
    return text;
}

bool SqlConnection::TransactionActive() const noexcept
{
    SQLUINTEGER state {};
    SQLRETURN sqlResult = SQLGetConnectAttrA(m_hDbc, SQL_ATTR_AUTOCOMMIT, &state, 0, nullptr);
    return sqlResult == SQL_SUCCESS && state == SQL_AUTOCOMMIT_OFF;
}

bool SqlConnection::TransactionsAllowed() const noexcept
{
    SQLUSMALLINT txn {};
    SQLSMALLINT t {};
    SQLRETURN const rv = SQLGetInfo(m_hDbc, (SQLUSMALLINT) SQL_TXN_CAPABLE, &txn, sizeof(txn), &t);
    return rv == SQL_SUCCESS && txn != SQL_TC_NONE;
}

bool SqlConnection::IsAlive() const noexcept
{
    SQLUINTEGER state {};
    SQLRETURN sqlResult = SQLGetConnectAttrA(m_hDbc, SQL_ATTR_CONNECTION_DEAD, &state, 0, nullptr);
    return SQL_SUCCEEDED(sqlResult) && state == SQL_CD_FALSE;
}

void SqlConnection::RequireSuccess(SQLRETURN error, std::source_location sourceLocation) const
{
    if (SQL_SUCCEEDED(error))
        return;

    auto errorInfo = SqlErrorInfo::fromConnectionHandle(m_hDbc);
    SqlLogger::GetLogger().OnError(errorInfo, sourceLocation);
    throw std::runtime_error(std::format("SQL error: {}", errorInfo));
}

SqlQueryBuilder SqlConnection::Query(std::string_view const& table) const
{
    return SqlQueryBuilder(QueryFormatter(), std::string(table));
}

SqlQueryBuilder SqlConnection::QueryAs(std::string_view const& table, std::string_view const& tableAlias) const
{
    return SqlQueryBuilder(QueryFormatter(), std::string(table), std::string(tableAlias));
}
