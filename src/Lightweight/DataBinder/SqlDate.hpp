// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

#include <chrono>

// Helper struct to store a date (without time of the day) to write to or read from a database.
struct SqlDate
{
    SQL_DATE_STRUCT sqlValue {};

    SqlDate() noexcept = default;
    SqlDate(SqlDate&&) noexcept = default;
    SqlDate& operator=(SqlDate&&) noexcept = default;
    SqlDate(SqlDate const&) noexcept = default;
    SqlDate& operator=(SqlDate const&) noexcept = default;
    ~SqlDate() noexcept = default;

    [[nodiscard]] std::chrono::year_month_day value() const noexcept
    {
        return ConvertToNative(sqlValue);
    }

    bool operator==(SqlDate const& other) const noexcept
    {
        return sqlValue.year == other.sqlValue.year && sqlValue.month == other.sqlValue.month
               && sqlValue.day == other.sqlValue.day;
    }

    bool operator!=(SqlDate const& other) const noexcept
    {
        return !(*this == other);
    }

    SqlDate(std::chrono::year_month_day value) noexcept:
        sqlValue { SqlDate::ConvertToSqlValue(value) }
    {
    }

    SqlDate(std::chrono::year year, std::chrono::month month, std::chrono::day day) noexcept:
        SqlDate(std::chrono::year_month_day { year, month, day })
    {
    }

    static SqlDate Today() noexcept
    {
        return SqlDate { std::chrono::year_month_day {
            std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()),
        } };
    }

    static SQL_DATE_STRUCT ConvertToSqlValue(std::chrono::year_month_day value) noexcept
    {
        return SQL_DATE_STRUCT {
            .year = (SQLSMALLINT) (int) value.year(),
            .month = (SQLUSMALLINT) (unsigned) value.month(),
            .day = (SQLUSMALLINT) (unsigned) value.day(),
        };
    }

    static std::chrono::year_month_day ConvertToNative(SQL_DATE_STRUCT const& value) noexcept
    {
        return std::chrono::year_month_day { std::chrono::year { value.year },
                                             std::chrono::month { static_cast<unsigned>(value.month) },
                                             std::chrono::day { static_cast<unsigned>(value.day) } };
    }
};

template <>
struct SqlDataBinder<SqlDate>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlDate const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_TYPE_DATE,
                                SQL_TYPE_DATE,
                                0,
                                0,
                                (SQLPOINTER) &value.sqlValue,
                                0,
                                nullptr);
    }

    static SQLRETURN OutputColumn(SQLHSTMT stmt,
                                  SQLUSMALLINT column,
                                  SqlDate* result,
                                  SQLLEN* indicator,
                                  SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindCol(stmt, column, SQL_C_TYPE_DATE, &result->sqlValue, sizeof(result->sqlValue), indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlDate* result, SQLLEN* indicator) noexcept
    {
        return SQLGetData(stmt, column, SQL_C_TYPE_DATE, &result->sqlValue, sizeof(result->sqlValue), indicator);
    }
};
