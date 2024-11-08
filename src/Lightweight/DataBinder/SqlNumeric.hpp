// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Primitives.hpp"

#include <cmath>
#include <compare>
#include <concepts>
#include <cstring>

// clang-format off
#if defined(__SIZEOF_INT128__)
    #define LIGHTWEIGHT_INT128_T __int128_t
    static_assert(sizeof(__int128_t) == SQL_MAX_NUMERIC_LEN);
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
        std::memcpy(sqlValue.val, &num, sizeof(num));
#else
        auto const num = static_cast<int64_t>(value * std::pow(10, Scale));
        std::memset(sqlValue.val, 0, sizeof(sqlValue.val));
        std::memcpy(sqlValue.val, &num, sizeof(num));
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

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE float ToFloat() const noexcept
    {
#if defined(LIGHTWEIGHT_INT128_T)
        return float(*reinterpret_cast<LIGHTWEIGHT_INT128_T const*>(sqlValue.val)) / std::pow(10, sqlValue.scale);
#else
        return float(*reinterpret_cast<int64_t const*>(sqlValue.val)) / std::pow(10, sqlValue.scale);
#endif
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE double ToDouble() const noexcept
    {
#if defined(LIGHTWEIGHT_INT128_T)
        return double(*reinterpret_cast<LIGHTWEIGHT_INT128_T const*>(sqlValue.val)) / std::pow(10, sqlValue.scale);
#else
        return double(*reinterpret_cast<int64_t const*>(sqlValue.val)) / std::pow(10, sqlValue.scale);
#endif
    }

    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::string ToString() const
    {
        return std::format("{:.{}f}", ToFloat(), 2);
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE std::weak_ordering operator<=>(
        SqlNumeric const& other) const noexcept
    {
        return ToDouble() <=> other.ToDouble();
    }

    [[nodiscard]] constexpr LIGHTWEIGHT_FORCE_INLINE bool operator==(SqlNumeric const& other) const noexcept
    {
        // clang-format off
        return sqlValue.sign == other.sqlValue.sign
               && sqlValue.scale == other.sqlValue.scale
               && std::strncmp((char*) sqlValue.val, (char*) other.sqlValue.val, sizeof(sqlValue.val)) == 0;
        // clang-format on
    }
};

// clang-format off
template <std::size_t Precision, std::size_t Scale>
struct SqlDataBinder<SqlNumeric<Precision, Scale>>
{
    using ValueType = SqlNumeric<Precision, Scale>;

    static constexpr SqlColumnType ColumnType = SqlColumnType::NUMERIC;

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, ValueType const& value, SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindParameter(stmt, column, SQL_PARAM_INPUT, SQL_C_NUMERIC, SQL_NUMERIC, Precision, Scale, (SQLPOINTER) &value.sqlValue, 0, nullptr);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN OutputColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback& /*unused*/) noexcept
    {
        return SQLBindCol(stmt, column, SQL_C_NUMERIC, &result->sqlValue, sizeof(ValueType), indicator);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback const& /*cb*/) noexcept
    {
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
