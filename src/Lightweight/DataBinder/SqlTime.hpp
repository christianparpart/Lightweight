// SPDX-License-Identifier: MIT
#pragma once

#include "Core.hpp"

#include <chrono>

// clang-format off
#if !defined(SQL_SS_TIME2)
// This is a Microsoft-specific extension to ODBC.
// It is supported by at lesat the following drivers:
// - SQL Server 2008 and later
// - MariaDB and MySQL ODBC drivers

#define SQL_SS_TIME2 (-154)

struct SQL_SS_TIME2_STRUCT
{
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
    SQLUINTEGER fraction;
};

static_assert(
    sizeof(SQL_SS_TIME2_STRUCT) == 12,
    "SQL_SS_TIME2_STRUCT size must be padded 12 bytes, as per ODBC extension spec."
);

#endif
// clang-format on

// Helper struct to store a time (of the day) to write to or read from a database.
struct SqlTime
{
    using native_type = std::chrono::hh_mm_ss<std::chrono::microseconds>;

#if defined(SQL_SS_TIME2)
    using sql_type = SQL_SS_TIME2_STRUCT;
#else
    using sql_type = SQL_TIME_STRUCT;
#endif

    sql_type sqlValue {};

    SqlTime() noexcept = default;
    SqlTime(SqlTime&&) noexcept = default;
    SqlTime& operator=(SqlTime&&) noexcept = default;
    SqlTime(SqlTime const&) noexcept = default;
    SqlTime& operator=(SqlTime const&) noexcept = default;
    ~SqlTime() noexcept = default;

    [[nodiscard]] native_type value() const noexcept
    {
        return ConvertToNative(sqlValue);
    }

    bool operator==(SqlTime const& other) const noexcept
    {
        return value().to_duration().count() == other.value().to_duration().count();
    }

    bool operator!=(SqlTime const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlTime(native_type value) noexcept:
        sqlValue { SqlTime::ConvertToSqlValue(value) }
    {
    }

    SqlTime(std::chrono::hours hour,
            std::chrono::minutes minute,
            std::chrono::seconds second,
            std::chrono::microseconds micros = {}) noexcept:
        SqlTime(native_type { hour + minute + second + micros })
    {
    }

    static sql_type ConvertToSqlValue(native_type value) noexcept
    {
        return sql_type {
            .hour = (SQLUSMALLINT) value.hours().count(),
            .minute = (SQLUSMALLINT) value.minutes().count(),
            .second = (SQLUSMALLINT) value.seconds().count(),
#if defined(SQL_SS_TIME2)
            .fraction = (SQLUINTEGER) value.subseconds().count(),
#endif
        };
    }

    static native_type ConvertToNative(sql_type const& value) noexcept
    {
        // clang-format off
        return native_type { std::chrono::hours { (int) value.hour }
                             + std::chrono::minutes { (unsigned) value.minute }
                             + std::chrono::seconds { (unsigned) value.second }
#if defined(SQL_SS_TIME2)
                             + std::chrono::microseconds { value.fraction }
#endif

        };
        // clang-format on
    }
};

template <>
struct SqlDataBinder<SqlTime>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlTime const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_TYPE_TIME,
                                SQL_TYPE_TIME,
                                0,
                                0,
                                (SQLPOINTER) &value.sqlValue,
                                0,
                                nullptr);
    }

    static SQLRETURN OutputColumn(SQLHSTMT stmt,
                                  SQLUSMALLINT column,
                                  SqlTime* result,
                                  SQLLEN* indicator,
                                  SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindCol(stmt, column, SQL_C_TYPE_TIME, &result->sqlValue, sizeof(result->sqlValue), indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlTime* result, SQLLEN* indicator) noexcept
    {
        return SQLGetData(stmt, column, SQL_C_TYPE_TIME, &result->sqlValue, sizeof(result->sqlValue), indicator);
    }
};
