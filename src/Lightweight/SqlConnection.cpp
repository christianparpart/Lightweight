// SPDX-License-Identifier: Apache-2.0

#include "SqlConnection.hpp"
#include "SqlQuery.hpp"
#include "SqlQueryFormatter.hpp"

#include <sql.h>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

static std::optional<SqlConnectInfo> gDefaultConnectInfo {};
static std::atomic<uint64_t> gNextConnectionId { 1 };
static std::function<void(SqlConnection&)> gPostConnectedHook {};

// =====================================================================================================================

struct SqlConnection::Data
{
    std::chrono::steady_clock::time_point lastUsed; // Last time the connection was used (mostly interesting for
                                                    // idle connections in the connection pool).
    SqlConnectInfo connectInfo;
};

SqlConnection::SqlConnection():
    m_connectionId { gNextConnectionId++ },
    m_data { new Data() }
{
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);
    SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hDbc);

    Connect(DefaultConnectInfo());
}

SqlConnection::SqlConnection(std::optional<SqlConnectInfo> connectInfo):
    m_connectionId { gNextConnectionId++ },
    m_data { new Data() }
{
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);
    SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hDbc);

    if (connectInfo)
        Connect(std::move(*connectInfo));
}

SqlConnection::SqlConnection(SqlConnection&& other) noexcept:
    m_hEnv { other.m_hEnv },
    m_hDbc { other.m_hDbc },
    m_connectionId { other.m_connectionId },
    m_serverType { other.m_serverType },
    m_queryFormatter { other.m_queryFormatter },
    m_data { other.m_data }
{
    other.m_hEnv = {};
    other.m_hDbc = {};
    other.m_data = nullptr;
}

SqlConnection& SqlConnection::operator=(SqlConnection&& other) noexcept
{
    if (this == &other)
        return *this;

    Close();

    m_hEnv = other.m_hEnv;
    m_hDbc = other.m_hDbc;
    m_connectionId = other.m_connectionId;
    m_data = other.m_data;

    other.m_hEnv = {};
    other.m_hDbc = {};
    other.m_data = nullptr;

    return *this;
}

SqlConnection::~SqlConnection() noexcept
{
    Close();
    delete m_data;
}

SqlConnectInfo const& SqlConnection::DefaultConnectInfo() noexcept
{
    return gDefaultConnectInfo.value();
}

void SqlConnection::SetDefaultConnectInfo(SqlConnectInfo connectInfo) noexcept
{
    gDefaultConnectInfo = std::move(connectInfo);
}

SqlConnectInfo const& SqlConnection::ConnectionInfo() const noexcept
{
    return m_data->connectInfo;
}

void SqlConnection::SetLastUsed(std::chrono::steady_clock::time_point lastUsed) noexcept
{
    m_data->lastUsed = lastUsed;
}

std::chrono::steady_clock::time_point SqlConnection::LastUsed() const noexcept
{
    return m_data->lastUsed;
}

void SqlConnection::SetPostConnectedHook(std::function<void(SqlConnection&)> hook)
{
    gPostConnectedHook = std::move(hook);
}

void SqlConnection::ResetPostConnectedHook()
{
    gPostConnectedHook = {};
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
    if (m_hDbc)
        SQLDisconnect(m_hDbc);

    m_data->connectInfo = std::move(connectInfo);

    if (auto const* info = std::get_if<SqlConnectionDataSource>(&m_data->connectInfo))
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

        if (gPostConnectedHook)
            gPostConnectedHook(*this);

        return true;
    }

    auto const& connectionString = std::get<SqlConnectionString>(m_data->connectInfo).value;

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

    if (gPostConnectedHook)
        gPostConnectedHook(*this);

    return true;
}

void SqlConnection::Close() noexcept
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
