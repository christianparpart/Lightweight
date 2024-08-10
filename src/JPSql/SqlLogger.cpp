#include "SqlConnection.hpp"
#include "SqlLogger.hpp"

#include <chrono>
#include <cstdlib>
#include <format>
#include <print>
#include <ranges>
#include <stacktrace>
#include <version>

#if defined(_MSC_VER)
    // Disable warning C4996: This function or variable may be unsafe.
    // It is complaining about getenv, which is fine to use in this case.
    #pragma warning(disable : 4996)
#endif

namespace
{

std::string ConnectInfoLogString(SqlConnectInfo const& info)
{
    return std::visit(
        [](auto const& info) -> std::string {
            using T = std::decay_t<decltype(info)>;
            if constexpr (std::is_same_v<T, SqlConnectionString>)
                return info.connectionString;
            else if constexpr (std::is_same_v<T, SqlConnectionDataSource>)
                return std::format("{}@{}", info.username, info.datasource);
            else
                static_assert(false, "non-exhaustive visitor!");
        },
        info);
}

class SqlStandardLogger: public SqlLogger
{
  private:
    std::chrono::time_point<std::chrono::system_clock> m_currentTime {};
    std::string m_currentTimeStr {};

  public:
    void Tick()
    {
        m_currentTime = std::chrono::system_clock::now();
        auto const nowMs = time_point_cast<std::chrono::milliseconds>(m_currentTime);
        m_currentTimeStr = std::format("{:%F %X}.{:03}", m_currentTime, nowMs.time_since_epoch().count() % 1'000);
    }

    template <typename... Args>
    void WriteMessage(std::format_string<Args...> const& fmt, Args&&... args)
    {
        // TODO: Use the new logging mechanism from Felix here, once merged.
        std::println("[{}] {}", m_currentTimeStr, std::format(fmt, std::forward<Args>(args)...));
    }

    void OnWarning(std::string_view const& message) override
    {
        Tick();
        WriteMessage("Warning: {}", message);
    }

    void OnError(SqlError errorCode, SqlErrorInfo const& errorInfo, std::source_location /*sourceLocation*/) override
    {
        Tick();
        WriteMessage("Error: {}", SqlErrorCategory().message((int) errorCode));
        WriteMessage("  SQLSTATE: {}", errorInfo.sqlState);
        WriteMessage("  Native error code: {}", errorInfo.nativeErrorCode);
        WriteMessage("  Message: {}", errorInfo.message);
    }

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

class SqlTraceLogger: public SqlStandardLogger
{
    std::string m_lastPreparedQuery;

  public:
    void OnError(SqlError errorCode, SqlErrorInfo const& errorInfo, std::source_location sourceLocation) override
    {
        SqlStandardLogger::OnError(errorCode, errorInfo, sourceLocation);

        WriteMessage("  Source: {}:{}", sourceLocation.file_name(), sourceLocation.line());
        WriteMessage("  Stack trace:");

        auto stackTrace = std::stacktrace::current(1, 25);
        for (std::size_t const i: std::views::iota(std::size_t(0), stackTrace.size()))
            WriteMessage("    [{:>2}] {}", i, stackTrace[i]);
    }

    void OnConnectionOpened(SqlConnection const& connection) override
    {
        Tick();
        WriteMessage(
            "Connection {} opened: {}", connection.ConnectionId(), ConnectInfoLogString(connection.ConnectionInfo()));
    }

    void OnConnectionClosed(SqlConnection const& connection) override
    {
        Tick();
        WriteMessage(
            "Connection {} closed: {}", connection.ConnectionId(), ConnectInfoLogString(connection.ConnectionInfo()));
    }

    void OnConnectionIdle(SqlConnection const& connection) override
    {
        Tick();
        WriteMessage(
            "Connection {} idle: {}", connection.ConnectionId(), ConnectInfoLogString(connection.ConnectionInfo()));
    }

    void OnConnectionReuse(SqlConnection const& connection) override
    {
        Tick();
        WriteMessage(
            "Connection {} reused: {}", connection.ConnectionId(), ConnectInfoLogString(connection.ConnectionInfo()));
    }

    void OnExecuteDirect(std::string_view const& query) override
    {
        Tick();
        WriteMessage("ExecuteDirect: {}", query);
    }

    void OnPrepare(std::string_view const& query) override
    {
        m_lastPreparedQuery = query;
    }

    void OnExecute() override
    {
        Tick();
        WriteMessage("Execute: {}", m_lastPreparedQuery);
    }

    void OnExecuteBatch() override
    {
        Tick();
        WriteMessage("ExecuteBatch: {}", m_lastPreparedQuery);
    }

    void OnFetchedRow() override
    {
        Tick();
        WriteMessage("Fetched row");
    }
};

} // namespace

SqlLogger& SqlLogger::StandardLogger()
{
    static SqlStandardLogger logger {};
    return logger;
}

SqlLogger& SqlLogger::TraceLogger()
{
    static SqlTraceLogger logger {};
    return logger;
}

static SqlLogger* theDefaultLogger = []() -> SqlLogger* {
    if (auto const* env = std::getenv("SQL_TRACE"); env && std::string_view(env) == "1")
        return &SqlLogger::TraceLogger();

    return &SqlLogger::StandardLogger();
}();

SqlLogger& SqlLogger::GetLogger()
{
    return *theDefaultLogger;
}

void SqlLogger::SetLogger(SqlLogger& logger)
{
    theDefaultLogger = &logger;
}
