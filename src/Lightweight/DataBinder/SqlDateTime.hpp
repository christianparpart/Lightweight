// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"
#include "../SqlColumnTypeDefinitions.hpp"

#include <chrono>
#include <format>

struct LIGHTWEIGHT_API SqlDateTime
{
    using native_type = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

    static SqlDateTime Now() noexcept
    {
        return SqlDateTime { std::chrono::system_clock::now() };
    }

    constexpr SqlDateTime() noexcept = default;
    constexpr SqlDateTime(SqlDateTime&&) noexcept = default;
    constexpr SqlDateTime& operator=(SqlDateTime&&) noexcept = default;
    constexpr SqlDateTime(SqlDateTime const&) noexcept = default;
    constexpr SqlDateTime& operator=(SqlDateTime const& other) noexcept = default;
    constexpr ~SqlDateTime() noexcept = default;

    constexpr bool operator==(SqlDateTime const& other) const noexcept
    {
        return value() == other.value();
    }

    constexpr bool operator!=(SqlDateTime const& other) const noexcept
    {
        return !(*this == other);
    }

    constexpr SqlDateTime(std::chrono::year_month_day ymd,
                          std::chrono::hh_mm_ss<std::chrono::nanoseconds> time) noexcept
    {
        sqlValue.year = (SQLSMALLINT) (int) ymd.year();
        sqlValue.month = (SQLUSMALLINT) (unsigned) ymd.month();
        sqlValue.day = (SQLUSMALLINT) (unsigned) ymd.day();
        sqlValue.hour = (SQLUSMALLINT) time.hours().count();
        sqlValue.minute = (SQLUSMALLINT) time.minutes().count();
        sqlValue.second = (SQLUSMALLINT) time.seconds().count();
        sqlValue.fraction = (SQLUINTEGER) (time.subseconds().count() / 100) * 100;
    }

    constexpr SqlDateTime(std::chrono::year year,
                          std::chrono::month month,
                          std::chrono::day day,
                          std::chrono::hours hour,
                          std::chrono::minutes minute,
                          std::chrono::seconds second,
                          std::chrono::nanoseconds nanosecond = std::chrono::nanoseconds { 0 }) noexcept
    {
        sqlValue.year = (SQLSMALLINT) (int) year;
        sqlValue.month = (SQLUSMALLINT) (unsigned) month;
        sqlValue.day = (SQLUSMALLINT) (unsigned) day;
        sqlValue.hour = (SQLUSMALLINT) hour.count();
        sqlValue.minute = (SQLUSMALLINT) minute.count();
        sqlValue.second = (SQLUSMALLINT) second.count();
        sqlValue.fraction = (SQLUINTEGER) (nanosecond.count() / 100) * 100;
    }

    constexpr SqlDateTime(std::chrono::system_clock::time_point value) noexcept:
        sqlValue { SqlDateTime::ConvertToSqlValue(value) }
    {
    }

    constexpr operator native_type() const noexcept
    {
        return value();
    }

    static SQL_TIMESTAMP_STRUCT constexpr ConvertToSqlValue(native_type value) noexcept
    {
        using namespace std::chrono;
        auto const totalDays = floor<days>(value);
        auto const ymd = year_month_day { totalDays };
        auto const hms = hh_mm_ss<nanoseconds> { floor<nanoseconds>(value - totalDays) };

        return SQL_TIMESTAMP_STRUCT {
            .year = (SQLSMALLINT) (int) ymd.year(),
            .month = (SQLUSMALLINT) (unsigned) ymd.month(),
            .day = (SQLUSMALLINT) (unsigned) ymd.day(),
            .hour = (SQLUSMALLINT) hms.hours().count(),
            .minute = (SQLUSMALLINT) hms.minutes().count(),
            .second = (SQLUSMALLINT) hms.seconds().count(),
            .fraction = (SQLUINTEGER) (hms.subseconds().count() / 100) * 100,
        };
    }

    static native_type constexpr ConvertToNative(SQL_TIMESTAMP_STRUCT const& time) noexcept
    {
        // clang-format off
        using namespace std::chrono;
        auto timepoint = sys_days(year_month_day(year(time.year), month(time.month), day(time.day)))
                       + hours(time.hour)
                       + minutes(time.minute)
                       + seconds(time.second)
                       + nanoseconds(time.fraction);
        return timepoint;
        // clang-format on
    }

    [[nodiscard]] constexpr native_type value() const noexcept
    {
        return ConvertToNative(sqlValue);
    }

    SQL_TIMESTAMP_STRUCT sqlValue {};
};

template <>
struct LIGHTWEIGHT_API SqlDataBinder<SqlDateTime::native_type>
{
    static SQLRETURN GetColumn(SQLHSTMT stmt,
                               SQLUSMALLINT column,
                               SqlDateTime::native_type* result,
                               SQLLEN* indicator,
                               SqlDataBinderCallback const& /*cb*/) noexcept
    {
        SQL_TIMESTAMP_STRUCT sqlValue {};
        auto const rc = SQLGetData(stmt, column, SQL_C_TYPE_TIMESTAMP, &sqlValue, sizeof(sqlValue), indicator);
        if (SQL_SUCCEEDED(rc))
            *result = SqlDateTime::ConvertToNative(sqlValue);
        return rc;
    }
};

template <>
struct LIGHTWEIGHT_API SqlDataBinder<SqlDateTime>
{
    static constexpr auto ColumnType = SqlColumnTypeDefinitions::DateTime {};

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt,
                                                             SQLUSMALLINT column,
                                                             SqlDateTime const& value,
                                                             SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_TIMESTAMP,
                                SQL_TYPE_TIMESTAMP,
                                27,
                                7,
                                (SQLPOINTER) &value.sqlValue,
                                sizeof(value),
                                nullptr);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN OutputColumn(SQLHSTMT stmt,
                                                           SQLUSMALLINT column,
                                                           SqlDateTime* result,
                                                           SQLLEN* indicator,
                                                           SqlDataBinderCallback& /*cb*/) noexcept
    {
        // TODO: handle indicator to check for NULL values
        *indicator = sizeof(result->sqlValue);
        return SQLBindCol(stmt, column, SQL_C_TYPE_TIMESTAMP, &result->sqlValue, 0, indicator);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN GetColumn(SQLHSTMT stmt,
                                                        SQLUSMALLINT column,
                                                        SqlDateTime* result,
                                                        SQLLEN* indicator,
                                                        SqlDataBinderCallback const& /*cb*/) noexcept
    {
        return SQLGetData(stmt, column, SQL_C_TYPE_TIMESTAMP, &result->sqlValue, sizeof(result->sqlValue), indicator);
    }

    static LIGHTWEIGHT_FORCE_INLINE std::string Inspect(SqlDateTime const& value) noexcept
    {
        return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
                           value.sqlValue.year,
                           value.sqlValue.month,
                           value.sqlValue.day,
                           value.sqlValue.hour,
                           value.sqlValue.minute,
                           value.sqlValue.second);
    }
};

template <>
struct std::formatter<SqlDateTime>: std::formatter<std::string>
{
    auto format(SqlDateTime const& value, std::format_context& ctx) const -> std::format_context::iterator
    {
        return std::formatter<std::string>::format(std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
                                                               value.sqlValue.year,
                                                               value.sqlValue.month,
                                                               value.sqlValue.day,
                                                               value.sqlValue.hour,
                                                               value.sqlValue.minute,
                                                               value.sqlValue.second),
                                                   ctx);
    }
};
