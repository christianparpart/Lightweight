#pragma once

#include "../JPSql/Model.hpp"
#include "../JPSql/SqlConnectInfo.hpp"
#include "../JPSql/SqlDataBinder.hpp"
#include "../JPSql/SqlLogger.hpp"

#include <format>
#include <ostream>
#include <string_view>

// Refer to an in-memory SQLite database (and assuming the sqliteodbc driver is installed)
// See:
// - https://www.sqlite.org/inmemorydb.html
// - http://www.ch-werner.de/sqliteodbc/
// - https://github.com/softace/sqliteodbc
//
auto const inline TestSqlConnectionString = SqlConnectionString
{
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
    SqlTestFixture()
    {
        SqlConnection::SetDefaultConnectInfo(TestSqlConnectionString);
    }

    virtual ~SqlTestFixture()
    {
        SqlConnection::KillAllIdle();
    }
};

class SqlModelTestFixture: public SqlTestFixture
{
  public:
    SqlModelTestFixture()
    {
    }

    ~SqlModelTestFixture() override
    {
    }
};

// {{{ ostream support for JPSql, for debugging purposes
inline std::ostream& operator<<(std::ostream& os, Model::RecordId value)
{
    return os << "ModelId { " << value.value << " }";
}

inline std::ostream& operator<<(std::ostream& os, SqlResult<void> const& result)
{
    if (result)
        return os << "SqlResult<void> { success }";
    return os << "SqlResult<void> { error: " << result.error() << " }";
}

inline std::ostream& operator<<(std::ostream& os, SqlDate const& date)
{
    auto const ymd = date.value;
    return os << std::format("SqlDate {{ {}-{}-{} }}", ymd.year(), ymd.month(), ymd.day());
}

inline std::ostream& operator<<(std::ostream& os, SqlTime const& time)
{
    return os << std::format("SqlTime {{ {:02}:{:02}:{:02} }}",
                             time.value.hours().count(),
                             time.value.minutes().count(),
                             time.value.seconds().count());
}

inline std::ostream& operator<<(std::ostream& os, SqlTimestamp const& timestamp)
{
    return os << std::format("SqlTimestamp {{ {} }}", timestamp.value);
}

template <typename T>
inline std::ostream& operator<<(std::ostream& os, SqlResult<T> const& result)
{
    if (result)
        return os << "SqlResult<int> { value: " << result.value() << " }";
    return os << "SqlResult<int> { error: " << result.error() << " }";
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
