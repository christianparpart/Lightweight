// SPDX-License-Identifier: MIT
#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #include <Windows.h>
#endif

#include "../JPSql/Model.hpp"
#include "../JPSql/SqlConnectInfo.hpp"
#include "../JPSql/SqlDataBinder.hpp"
#include "../JPSql/SqlLogger.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <expected>
#include <format>
#include <ostream>
#include <regex>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <sql.h>
#include <sqlext.h>
#include <sqlspi.h>
#include <sqltypes.h>

// Refer to an in-memory SQLite database (and assuming the sqliteodbc driver is installed)
// See:
// - https://www.sqlite.org/inmemorydb.html
// - http://www.ch-werner.de/sqliteodbc/
// - https://github.com/softace/sqliteodbc
//
auto const inline DefaultTestConnectionString = SqlConnectionString {
    .connectionString = std::format("DRIVER={};Database={}",
#if defined(_WIN32) || defined(_WIN64)
                                    "SQLite3 ODBC Driver",
#else
                                    "SQLite3",
#endif
                                    "file::memory:"),
};

class ScopedSqlNullLogger: public SqlLogger
{
  private:
    SqlLogger& m_previousLogger = SqlLogger::GetLogger();

  public:
    ScopedSqlNullLogger()
    {
        SqlLogger::SetLogger(*this);
    }

    ~ScopedSqlNullLogger() override
    {
        SqlLogger::SetLogger(m_previousLogger);
    }

    void OnWarning(std::string_view const&) override {}
    void OnError(SqlError, SqlErrorInfo const&, std::source_location) override {}
    void OnConnectionOpened(SqlConnection const&) override {}
    void OnConnectionClosed(SqlConnection const&) override {}
    void OnConnectionIdle(SqlConnection const&) override {}
    void OnConnectionReuse(SqlConnection const&) override {}
    void OnExecuteDirect(std::string_view const&) override {}
    void OnPrepare(std::string_view const&) override {}
    void OnExecute() override {}
    void OnExecuteBatch() override {}
    void OnFetchedRow() override {}
};

class SqlTestFixture
{
  public:
    static inline std::string_view const testDatabaseName = "JPSqlTest";

    using MainProgramArgs = std::tuple<int, char**>;

    static std::expected<MainProgramArgs, int> Initialize(int argc, char** argv)
    {
        using namespace std::string_view_literals;
        int i = 1;
        for (; i < argc; ++i)
        {
            if (argv[i] == "--trace-sql"sv)
                SqlLogger::SetLogger(SqlLogger::TraceLogger());
            else if (argv[i] == "--trace-model"sv)
                Model::QueryLogger::Set(Model::QueryLogger::StandardLogger());
            else if (argv[i] == "--help"sv || argv[i] == "-h"sv)
            {
                std::println("{} [--trace-sql] [--trace-model] [[--] [Catch2 flags ...]]", argv[0]);
                return std::unexpected { EXIT_SUCCESS };
            }
            else if (argv[i] == "--"sv)
            {
                ++i;
                break;
            }
            else
                break;
        }

        if (i < argc)
            argv[i - 1] = argv[0];

        if (auto const* s = std::getenv("ODBC_CONNECTION_STRING"); s && *s)
        {
            std::println("Using ODBC connection string: '{}'", SanitizePwd(s));
            SqlConnection::SetDefaultConnectInfo(SqlConnectionString { s });
        }
        else
        {
            // Use an in-memory SQLite3 database by default (for testing purposes)
            std::println("Using default ODBC connection string: '{}'", DefaultTestConnectionString.connectionString);
            SqlConnection::SetDefaultConnectInfo(DefaultTestConnectionString);
        }

        auto sqlConnection = SqlConnection();
        std::println("Running test cases against: {} ({}) (identified as: {})",
                     sqlConnection.ServerName(),
                     sqlConnection.ServerVersion(),
                     sqlConnection.ServerType());

        SqlConnection::SetPostConnectedHook(&SqlTestFixture::PostConnectedHook);

        return MainProgramArgs { argc - (i - 1), argv + (i - 1) };
    }

    static void PostConnectedHook(SqlConnection& connection)
    {
        switch (connection.ServerType())
        {
            case SqlServerType::SQLITE: {
                auto stmt = SqlStatement { connection };
                // Enable foreign key constraints for SQLite
                (void) stmt.ExecuteDirect("PRAGMA foreign_keys = ON");
                break;
            }
            case SqlServerType::MICROSOFT_SQL:
            case SqlServerType::POSTGRESQL:
            case SqlServerType::ORACLE:
            case SqlServerType::MYSQL:
            case SqlServerType::UNKNOWN:
                break;
        }
    }

    SqlTestFixture()
    {
        REQUIRE(SqlConnection().IsAlive());
        DropAllTablesInDatabase();
        SqlConnection::KillAllIdle();
    }

    virtual ~SqlTestFixture()
    {
        SqlConnection::KillAllIdle();
    }

    template <typename T>
    void CreateModelTable()
    {
        auto const tableName = T().TableName();
        m_createdTables.emplace_back(tableName);
        T::CreateTable();
    }

  private:
    static std::string SanitizePwd(std::string_view input)
    {
        std::regex const pwdRegex {
            R"(PWD=.*?;)",
            std::regex_constants::ECMAScript | std::regex_constants::icase,
        };
        std::stringstream outputString;
        std::regex_replace(
            std::ostreambuf_iterator<char> { outputString }, input.begin(), input.end(), pwdRegex, "Pwd=***;");
        return outputString.str();
    }

    std::vector<std::string> GetAllTableNames()
    {
        auto result = std::vector<std::string>();
        auto stmt = SqlStatement();
        auto const sqlResult = SQLTables(stmt.NativeHandle(),
                                         (SQLCHAR*) testDatabaseName.data(),
                                         (SQLSMALLINT) testDatabaseName.size(),
                                         nullptr,
                                         0,
                                         nullptr,
                                         0,
                                         (SQLCHAR*) "TABLE",
                                         SQL_NTS);
        if (SQL_SUCCEEDED(sqlResult))
        {
            while (stmt.FetchRow())
            {
                result.emplace_back(stmt.GetColumn<std::string>(3)); // table name
            }
        }
        return result;
    }

    void DropAllTablesInDatabase()
    {
        auto stmt = SqlStatement {};

        switch (stmt.Connection().ServerType())
        {
            case SqlServerType::MICROSOFT_SQL:
                SqlConnection::KillAllIdle();
                (void) stmt.ExecuteDirect(std::format("USE {}", "master"));
                (void) stmt.ExecuteDirect(std::format("DROP DATABASE IF EXISTS \"{}\"", testDatabaseName));
                (void) stmt.ExecuteDirect(std::format("CREATE DATABASE \"{}\"", testDatabaseName));
                (void) stmt.ExecuteDirect(std::format("USE {}", testDatabaseName));
                break;
            case SqlServerType::POSTGRESQL:
                if (m_createdTables.empty())
                    m_createdTables = GetAllTableNames();
                for (auto i = m_createdTables.rbegin(); i != m_createdTables.rend(); ++i)
                    (void) stmt.ExecuteDirect(std::format("DROP TABLE IF EXISTS \"{}\" CASCADE", *i));
                break;
            default:
                for (auto i = m_createdTables.rbegin(); i != m_createdTables.rend(); ++i)
                    (void) stmt.ExecuteDirect(std::format("DROP TABLE IF EXISTS \"{}\"", *i));
                break;
        }
        m_createdTables.clear();
    }

    std::vector<std::string> m_createdTables;
};

// {{{ ostream support for JPSql, for debugging purposes
inline std::ostream& operator<<(std::ostream& os, Model::RecordId value)
{
    return os << "ModelId { " << value.value << " }";
}

inline std::ostream& operator<<(std::ostream& os, Model::AbstractRecord const& value)
{
    return os << std::format("{}", value);
}

inline std::ostream& operator<<(std::ostream& os, SqlResult<void> const& result)
{
    if (result)
        return os << "SqlResult<void> { success }";
    return os << "SqlResult<void> { error: " << result.error() << " }";
}

inline std::ostream& operator<<(std::ostream& os, SqlTrimmedString const& value)
{
    return os << std::format("SqlTrimmedString {{ '{}' }}", value);
}

inline std::ostream& operator<<(std::ostream& os, SqlDate const& date)
{
    auto const ymd = date.value();
    return os << std::format("SqlDate {{ {}-{}-{} }}", ymd.year(), ymd.month(), ymd.day());
}

inline std::ostream& operator<<(std::ostream& os, SqlTime const& time)
{
    auto const value = time.value();
    return os << std::format("SqlTime {{ {:02}:{:02}:{:02}.{:06} }}",
                             value.hours().count(),
                             value.minutes().count(),
                             value.seconds().count(),
                             value.subseconds().count());
}

inline std::ostream& operator<<(std::ostream& os, SqlDateTime const& datetime)
{
    auto const value = datetime.value();
    auto const totalDays = std::chrono::floor<std::chrono::days>(value);
    auto const ymd = std::chrono::year_month_day { totalDays };
    auto const hms = std::chrono::hh_mm_ss<std::chrono::nanoseconds> { std::chrono::floor<std::chrono::nanoseconds>(
        value - totalDays) };
    return os << std::format("SqlDateTime {{ {:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:09} }}",
                             (int) ymd.year(),
                             (unsigned) ymd.month(),
                             (unsigned) ymd.day(),
                             hms.hours().count(),
                             hms.minutes().count(),
                             hms.seconds().count(),
                             hms.subseconds().count());
}

template <typename T>
inline std::ostream& operator<<(std::ostream& os, SqlResult<T> const& result)
{
    if (result)
        return os << "SqlResult<int> { value: " << result.value() << " }";
    return os << "SqlResult<int> { error: " << result.error() << " }";
}

template <std::size_t N, typename T, SqlStringPostRetrieveOperation PostOp>
inline std::ostream& operator<<(std::ostream& os, SqlFixedString<N, T, PostOp> const& value)
{
    if constexpr (PostOp == SqlStringPostRetrieveOperation::NOTHING)
        return os << std::format("SqlFixedString<{}> {{ '{}' }}", N, value.data());
    if constexpr (PostOp == SqlStringPostRetrieveOperation::TRIM_RIGHT)
        return os << std::format("SqlTrimmedFixedString<{}> {{ '{}' }}", N, value.data());
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Model::StringLiteral TheColumnName,
          Model::FieldValueRequirement TheRequirement>
inline std::ostream& operator<<(std::ostream& os,
                                Model::Field<T, TheTableColumnIndex, TheColumnName, TheRequirement> const& field)
{
    return os << std::format("Field<{}:{}: {}>", TheTableColumnIndex, TheColumnName.value, field.Value());
}

// }}}
