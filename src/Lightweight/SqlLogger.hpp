// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Api.hpp"
#include "SqlDataBinder.hpp"
#include "SqlError.hpp"

#include <source_location>
#include <string_view>

class SqlConnection;

struct SqlVariant;

// Represents a logger for SQL operations.
class LIGHTWEIGHT_API SqlLogger
{
  public:
    enum class SupportBindLogging
    {
        No,
        Yes
    };

    SqlLogger() = default;
    SqlLogger(SqlLogger const& /*other*/) = default;
    SqlLogger(SqlLogger&& /*other*/) = default;
    SqlLogger& operator=(SqlLogger const& /*other*/) = default;
    SqlLogger& operator=(SqlLogger&& /*other*/) = default;
    virtual ~SqlLogger() = default;

    explicit SqlLogger(SupportBindLogging supportBindLogging):
        _supportsBindLogging { supportBindLogging == SupportBindLogging::Yes }
    {
    }

    // Logs a warning message.
    virtual void OnWarning(std::string_view const& message) = 0;

    // An ODBC SQL error occurred.
    virtual void OnError(SqlError errorCode, std::source_location sourceLocation = std::source_location::current()) = 0;
    virtual void OnError(SqlErrorInfo const& errorInfo,
                         std::source_location sourceLocation = std::source_location::current()) = 0;

    // connection level events

    virtual void OnConnectionOpened(SqlConnection const& connection) = 0;
    virtual void OnConnectionClosed(SqlConnection const& connection) = 0;
    virtual void OnConnectionIdle(SqlConnection const& connection) = 0;
    virtual void OnConnectionReuse(SqlConnection const& connection) = 0;

    // statement level events

    virtual void OnExecuteDirect(std::string_view const& query) = 0;
    virtual void OnPrepare(std::string_view const& query) = 0;

    template <typename T>
    void OnBindInputParameter(std::string_view const& name, T&& value)
    {
        if (_supportsBindLogging)
        {
            using value_type = std::remove_cvref_t<T>;
            if constexpr (SqlDataBinderSupportsInspect<value_type>)
            {
                OnBind(name, std::string(SqlDataBinder<value_type>::Inspect(std::forward<T>(value))));
            }
        }
    }

    virtual void OnBind(std::string_view const& name, std::string value) = 0;
    virtual void OnExecute(std::string_view const& query) = 0;
    virtual void OnExecuteBatch() = 0;
    virtual void OnFetchRow() = 0;
    virtual void OnFetchEnd() = 0;

    class Null;

    // Logs nothing.
    static Null& NullLogger() noexcept;

    // Logs the most important events to standard output in a human-readable format.
    static SqlLogger& StandardLogger();

    // Logs every little event to standard output in a human-readable compact format.
    static SqlLogger& TraceLogger();

    // Retrieves the current logger.
    static SqlLogger& GetLogger();

    // Sets the current logger.
    // The ownership of the logger is not transferred and remains with the caller.
    static void SetLogger(SqlLogger& logger);

  private:
    bool _supportsBindLogging = false;
};

class SqlLogger::Null: public SqlLogger
{
  public:
    void OnWarning(std::string_view const& /*message*/) override {}
    void OnError(SqlError /*errorCode*/, std::source_location /*sourceLocation*/) override {}
    void OnError(SqlErrorInfo const& /*errorInfo*/, std::source_location /*sourceLocation*/) override {}
    void OnConnectionOpened(SqlConnection const& /*connection*/) override {}
    void OnConnectionClosed(SqlConnection const& /*connection*/) override {}
    void OnConnectionIdle(SqlConnection const& /*connection*/) override {}
    void OnConnectionReuse(SqlConnection const& /*connection*/) override {}
    void OnExecuteDirect(std::string_view const& /*query*/) override {}
    void OnPrepare(std::string_view const& /*qurey*/) override {}
    void OnBind(std::string_view const& /*name*/, std::string /*value*/) override {}
    void OnExecute(std::string_view const& /*query*/) override {}
    void OnExecuteBatch() override {}
    void OnFetchRow() override {}
    void OnFetchEnd() override {}
};
