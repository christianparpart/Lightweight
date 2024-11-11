// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../SqlError.hpp"
#include "../SqlLogger.hpp"
#include "Primitives.hpp"

#include <cmath>
#include <compare>
#include <concepts>
#include <cstring>
#include <print>

// clang-format off
#if defined(__SIZEOF_INT128__)
    #define LIGHTWEIGHT_INT128_T __int128_t
    static_assert(sizeof(__int128_t) == sizeof(SQL_NUMERIC_STRUCT::val));
#endif
// clang-format on

// Represents a fixed-point number with a given precision and scale.
//
// Precision is *exactly* the total number of digits in the number,
// including the digits after the decimal point.
//
// Scale is the number of digits after the decimal point.
template <std::size_t ThePrecision, std::size_t TheScale>
struct SqlNumeric
{
    // Number of total digits
    static constexpr auto Precision = ThePrecision;

    // Number of digits after the decimal point
    static constexpr auto Scale = TheScale;

    static_assert(TheScale < SQL_MAX_NUMERIC_LEN);
    static_assert(Scale <= Precision);

    // The value is stored as a string to avoid floating point precision issues.
    SQL_NUMERIC_STRUCT sqlValue {};

    constexpr SqlNumeric() noexcept = default;
    constexpr SqlNumeric(SqlNumeric&&) noexcept = default;
    constexpr SqlNumeric& operator=(SqlNumeric&&) noexcept = default;
    constexpr SqlNumeric(SqlNumeric const&) noexcept = default;
    constexpr SqlNumeric& operator=(SqlNumeric const&) noexcept = default;
    constexpr ~SqlNumeric() noexcept = default;

    constexpr SqlNumeric(std::floating_point auto value) noexcept
    {
        assign(value);
    }

    // For encoding/decoding purposes, we assume little-endian.
    static_assert(std::endian::native == std::endian::little);

    // Assigns a value to the numeric.
    LIGHTWEIGHT_FORCE_INLINE constexpr void assign(std::floating_point auto value) noexcept
    {
#if defined(LIGHTWEIGHT_INT128_T)
        auto const num = static_cast<LIGHTWEIGHT_INT128_T>(value * std::pow(10, Scale));
        *((LIGHTWEIGHT_INT128_T*) sqlValue.val) = num;
#else
        auto const num = static_cast<int64_t>(value * std::pow(10, Scale));
        std::memset(sqlValue.val, 0, sizeof(sqlValue.val));
        *((int64_t*) sqlValue.val) = num;
#endif

        sqlValue.sign = num >= 0; // 1 == positive, 0 == negative
        sqlValue.precision = Precision;
        sqlValue.scale = Scale;
    }

    LIGHTWEIGHT_FORCE_INLINE constexpr SqlNumeric& operator=(std::floating_point auto value) noexcept
    {
        assign(value);
        return *this;
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE auto ToUnscaledValue() const noexcept
    {
#if defined(LIGHTWEIGHT_INT128_T)
        return *reinterpret_cast<LIGHTWEIGHT_INT128_T const*>(sqlValue.val);
#else
        return *reinterpret_cast<int64_t const*>(sqlValue.val);
#endif
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE float ToFloat() const noexcept
    {
        return float(ToUnscaledValue()) / std::pow(10, Scale);
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE double ToDouble() const noexcept
    {
        return double(ToUnscaledValue()) / std::pow(10, Scale);
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE long double ToLongDouble() const noexcept
    {
        return static_cast<long double>(ToUnscaledValue()) / std::pow(10, Scale);
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE explicit operator float() const noexcept
    {
        return ToFloat();
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE explicit operator double() const noexcept
    {
        return ToDouble();
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE explicit operator long double() const noexcept
    {
        return ToLongDouble();
    }

    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::string ToString() const
    {
        return std::format("{:.{}f}", ToFloat(), Scale);
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE std::weak_ordering operator<=>(
        SqlNumeric const& other) const noexcept
    {
        return ToDouble() <=> other.ToDouble();
    }

    template <std::size_t OtherPrecision, std::size_t OtherScale>
    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE bool operator==(
        SqlNumeric<OtherPrecision, OtherScale> const& other) const noexcept
    {
        return ToFloat() == other.ToFloat();
    }
};

// clang-format off
template <std::size_t Precision, std::size_t Scale>
struct SqlDataBinder<SqlNumeric<Precision, Scale>>
{
    using ValueType = SqlNumeric<Precision, Scale>;

    static constexpr SqlColumnType ColumnType = SqlColumnType::NUMERIC;

    static void RequireSuccess(SQLHSTMT stmt, SQLRETURN error, std::source_location sourceLocation = std::source_location::current())
    {
        if (SQL_SUCCEEDED(error))
            return;

        auto errorInfo = SqlErrorInfo::fromStatementHandle(stmt);
        SqlLogger::GetLogger().OnError(errorInfo, sourceLocation);
        throw SqlException(std::move(errorInfo));
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, ValueType const& value, SqlDataBinderCallback& /*cb*/) noexcept
    {
        auto* mut = const_cast<ValueType*>(&value);
        mut->sqlValue.precision = Precision;
        mut->sqlValue.scale = Scale;
        RequireSuccess(stmt, SQLBindParameter(stmt, column, SQL_PARAM_INPUT, SQL_C_NUMERIC, SQL_NUMERIC, Precision, Scale, (SQLPOINTER) &value.sqlValue, 0, nullptr));
        return SQL_SUCCESS;
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN OutputColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback& /*unused*/) noexcept
    {
        SQLHDESC hDesc {};
        RequireSuccess(stmt, SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, (SQLPOINTER) &hDesc, 0, nullptr));
        RequireSuccess(stmt, SQLSetDescField(hDesc, (SQLSMALLINT) column, SQL_DESC_TYPE, (SQLPOINTER) SQL_NUMERIC, 0));
        RequireSuccess(stmt, SQLSetDescField(hDesc, (SQLSMALLINT) column, SQL_DESC_PRECISION, (SQLPOINTER) Precision, 0));
        RequireSuccess(stmt, SQLSetDescField(hDesc, (SQLSMALLINT) column, SQL_DESC_SCALE, (SQLPOINTER) Scale, 0));

        return SQLBindCol(stmt, column, SQL_C_NUMERIC, &result->sqlValue, sizeof(ValueType), indicator);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback const& /*cb*/) noexcept
    {
        SQLHDESC hDesc {};
        RequireSuccess(stmt, SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, (SQLPOINTER) &hDesc, 0, nullptr));
        RequireSuccess(stmt, SQLSetDescField(hDesc, (SQLSMALLINT) column, SQL_DESC_TYPE, (SQLPOINTER) SQL_NUMERIC, 0));
        RequireSuccess(stmt, SQLSetDescField(hDesc, (SQLSMALLINT) column, SQL_DESC_PRECISION, (SQLPOINTER) Precision, 0));
        RequireSuccess(stmt, SQLSetDescField(hDesc, (SQLSMALLINT) column, SQL_DESC_SCALE, (SQLPOINTER) Scale, 0));
        //RequireSuccess(stmt, SQLSetDescField(hDesc, (SQLSMALLINT) column, SQL_DESC_DATA_PTR, (SQLPOINTER) &result->sqlValue, sizeof(ValueType)));

        return SQLGetData(stmt, column, SQL_C_NUMERIC, &result->sqlValue, sizeof(ValueType), indicator);
    }
};
// clang-format off


template <std::size_t Precision, std::size_t Scale>
struct std::formatter<SqlNumeric<Precision, Scale>>: std::formatter<double>
{
    template <typename FormatContext>
    auto format(SqlNumeric<Precision, Scale> const& value, FormatContext& ctx)
    {
        return formatter<double>::format(value.ToDouble(), ctx);
    }
};