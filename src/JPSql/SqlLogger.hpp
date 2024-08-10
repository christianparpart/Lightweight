#pragma once

#include "SqlConnectInfo.hpp"
#include "SqlError.hpp"

#include <source_location>
#include <string_view>
#include <system_error>

class SqlConnection;

class SqlLogger
{
  public:
    virtual ~SqlLogger() = default;

    virtual void OnWarning(std::string_view const& message) = 0;
    virtual void OnError(SqlError errorCode,
                         SqlErrorInfo const& errorInfo,
                         std::source_location sourceLocation = std::source_location::current()) = 0;
    virtual void OnConnectionOpened(SqlConnection const& connection) = 0;
    virtual void OnConnectionClosed(SqlConnection const& connection) = 0;
    virtual void OnConnectionIdle(SqlConnection const& connection) = 0;
    virtual void OnConnectionReuse(SqlConnection const& connection) = 0;
    virtual void OnExecuteDirect(std::string_view const& query) = 0;
    virtual void OnPrepare(std::string_view const& query) = 0;
    virtual void OnExecute() = 0;
    virtual void OnExecuteBatch() = 0;
    virtual void OnFetchedRow() = 0;

    static SqlLogger& StandardLogger();
    static SqlLogger& TraceLogger();

    static SqlLogger& GetLogger();
    static void SetLogger(SqlLogger& logger);
};
