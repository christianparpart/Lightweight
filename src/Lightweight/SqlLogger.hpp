#pragma once

#include "SqlError.hpp"

#include <source_location>
#include <string_view>

class SqlConnection;

struct SqlConnectionStats
{
    size_t created {};
    size_t reused {};
    size_t closed {};
    size_t timedout {};
    size_t released {};
};

// Represents a logger for SQL operations.
class SqlLogger
{
  public:
    virtual ~SqlLogger() = default;

    // Logs a warning message.
    virtual void OnWarning(std::string_view const& message) = 0;

    // An ODBC SQL error occurred.
    virtual void OnError(SqlError errorCode, std::source_location sourceLocation = std::source_location::current()) = 0;
    virtual void OnError(SqlErrorInfo const& errorInfo,
                         std::source_location sourceLocation = std::source_location::current()) = 0;

    virtual void OnConnectionOpened(SqlConnection const& connection) = 0;
    virtual void OnConnectionClosed(SqlConnection const& connection) = 0;
    virtual void OnConnectionIdle(SqlConnection const& connection) = 0;
    virtual void OnConnectionReuse(SqlConnection const& connection) = 0;
    virtual void OnExecuteDirect(std::string_view const& query) = 0;
    virtual void OnPrepare(std::string_view const& query) = 0;
    virtual void OnExecute(std::string_view const& query) = 0;
    virtual void OnExecuteBatch() = 0;
    virtual void OnFetchedRow() = 0;
    virtual void OnStats(SqlConnectionStats const& stats) = 0;

    // Logs the most important events to standard output in a human-readable format.
    static SqlLogger& StandardLogger();

    // Logs every little event to standard output in a human-readable compact format.
    static SqlLogger& TraceLogger();

    // Retrieves the current logger.
    static SqlLogger& GetLogger();

    // Sets the current logger.
    // The ownership of the logger is not transferred and remains with the caller.
    static void SetLogger(SqlLogger& logger);
};
