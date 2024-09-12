#include "SqlConnection.hpp"
#include "SqlQueryFormatter.hpp"

#include <list>
#include <mutex>
#include <print>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

static std::list<SqlConnection> g_unusedConnections;

class SqlConnectionPool
{
  public:
    ~SqlConnectionPool()
    {
        KillAllIdleConnections();

        std::println(
            "SqlConnectionPool: Tearing down. (created: {}, reused: {}, closed: {}, timedout: {}, released: {})",
            m_stats.created,
            m_stats.reused,
            m_stats.closed,
            m_stats.timedout,
            m_stats.released);
    }

    void KillAllIdleConnections()
    {
        auto const _ = std::lock_guard { m_unusedConnectionsMutex };
        for (auto& connection: m_unusedConnections)
            connection.Kill();
        m_unusedConnections.clear();
    }

    SqlResult<SqlConnection> Acquire()
    {
        auto connection = AcquireDirect();
        if (connection.LastError() != SqlError::SUCCESS)
            return std::unexpected { connection.LastError() };
        return { std::move(connection) };
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

  private:
    std::list<SqlConnection> m_unusedConnections;
    std::mutex m_unusedConnectionsMutex;
    size_t m_maxIdleConnections = 10;
    std::chrono::seconds m_connectionTimeout = std::chrono::seconds { 120 };
    struct
    {
        size_t created {};
        size_t reused {};
        size_t closed {};
        size_t timedout {};
        size_t released {};
    } m_stats;
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
    m_lastError { other.m_lastError },
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
    m_lastError = std::move(other.m_lastError);
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

bool SqlConnection::Connect(std::string_view datasource, std::string_view username, std::string_view password) noexcept
{
    return Connect(SqlConnectionDataSource {
        .datasource = std::string(datasource), .username = std::string(username), .password = std::string(password) });
}

// Connects to the given database with the given ODBC connection string.
bool SqlConnection::Connect(std::string connectionString) noexcept
{
    return Connect(SqlConnectionString { .connectionString = std::move(connectionString) });
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
        UpdateLastError(SQLSetConnectAttrA(m_hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER) info->timeout.count(), 0));
        return UpdateLastError(SQLConnectA(m_hDbc,
                                           (SQLCHAR*) info->datasource.data(),
                                           (SQLSMALLINT) info->datasource.size(),
                                           (SQLCHAR*) info->username.data(),
                                           (SQLSMALLINT) info->username.size(),
                                           (SQLCHAR*) info->password.data(),
                                           (SQLSMALLINT) info->password.size()))
            .and_then([&] {
                return UpdateLastError(
                    SQLSetConnectAttrA(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER));
            })
            .and_then([&]() -> SqlResult<void> {
                PostConnect();
                SqlLogger::GetLogger().OnConnectionOpened(*this);
                if (m_gPostConnectedHook)
                    m_gPostConnectedHook(*this);
                return {};
            })
            .or_else([&](auto&&) -> SqlResult<void> {
                SqlLogger::GetLogger().OnError(m_lastError, SqlErrorInfo::fromConnectionHandle(m_hDbc));
                return std::unexpected { m_lastError };
            })
            .has_value();
    }

    auto const& connectionString = std::get<SqlConnectionString>(m_connectInfo).connectionString;

    return UpdateLastError(SQLDriverConnectA(m_hDbc,
                                             (SQLHWND) nullptr,
                                             (SQLCHAR*) connectionString.data(),
                                             (SQLSMALLINT) connectionString.size(),
                                             nullptr,
                                             0,
                                             nullptr,
                                             SQL_DRIVER_NOPROMPT))
        .and_then([&] {
            return UpdateLastError(
                SQLSetConnectAttrA(m_hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, SQL_IS_UINTEGER));
        })
        .and_then([&]() -> SqlResult<void> {
            PostConnect();
            SqlLogger::GetLogger().OnConnectionOpened(*this);
            if (m_gPostConnectedHook)
                m_gPostConnectedHook(*this);
            return {};
        })
        .has_value();
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
    UpdateLastError(SQLGetConnectAttrA(m_hDbc, SQL_ATTR_AUTOCOMMIT, &state, 0, nullptr));
    return m_lastError == SqlError::SUCCESS && state == SQL_AUTOCOMMIT_OFF;
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
    UpdateLastError(SQLGetConnectAttrA(m_hDbc, SQL_ATTR_CONNECTION_DEAD, &state, 0, nullptr));
    return m_lastError == SqlError::SUCCESS && state == SQL_CD_FALSE;
}

void SqlConnection::RequireSuccess(SQLRETURN error, std::source_location sourceLocation) const
{
    auto result = detail::UpdateSqlError(&m_lastError, error);
    if (result.has_value())
        return;

    auto errorInfo = SqlErrorInfo::fromConnectionHandle(m_hDbc);
    SqlLogger::GetLogger().OnError(m_lastError, errorInfo, sourceLocation);
    throw std::runtime_error(std::format("SQL error: {}", errorInfo));
}

SqlResult<void> SqlConnection::UpdateLastError(SQLRETURN error, std::source_location sourceLocation) const noexcept
{
    return detail::UpdateSqlError(&m_lastError, error).or_else([&](auto&&) -> SqlResult<void> {
        SqlLogger::GetLogger().OnError(m_lastError, SqlErrorInfo::fromConnectionHandle(m_hDbc), sourceLocation);
        return std::unexpected { m_lastError };
    });
}
