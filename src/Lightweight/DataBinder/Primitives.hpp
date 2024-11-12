// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

#include <concepts>

// clang-format off
template <typename T, SQLSMALLINT TheCType, SQLINTEGER TheSqlType, SqlColumnType TheColumnType>
struct SqlSimpleDataBinder
{
    static constexpr SqlColumnType ColumnType = TheColumnType;

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, T const& value, SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindParameter(stmt, column, SQL_PARAM_INPUT, TheCType, TheSqlType, 0, 0, (SQLPOINTER) &value, 0, nullptr);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN OutputColumn(SQLHSTMT stmt, SQLUSMALLINT column, T* result, SQLLEN* indicator, SqlDataBinderCallback& /*unused*/) noexcept
    {
        return SQLBindCol(stmt, column, TheCType, result, 0, indicator);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, T* result, SQLLEN* indicator, SqlDataBinderCallback const& /*cb*/) noexcept
    {
        return SQLGetData(stmt, column, TheCType, result, 0, indicator);
    }
};

template <> struct SqlDataBinder<bool>: SqlSimpleDataBinder<bool, SQL_BIT, SQL_BIT, SqlColumnType::BOOLEAN> {};
template <> struct SqlDataBinder<char>: SqlSimpleDataBinder<char, SQL_C_CHAR, SQL_CHAR, SqlColumnType::CHAR> {};
template <> struct SqlDataBinder<int16_t>: SqlSimpleDataBinder<int16_t, SQL_C_SSHORT, SQL_SMALLINT, SqlColumnType::SMALLINT> {};
template <> struct SqlDataBinder<uint16_t>: SqlSimpleDataBinder<uint16_t, SQL_C_USHORT, SQL_SMALLINT, SqlColumnType::SMALLINT> {};
template <> struct SqlDataBinder<int32_t>: SqlSimpleDataBinder<int32_t, SQL_C_SLONG, SQL_INTEGER, SqlColumnType::INTEGER> {};
template <> struct SqlDataBinder<uint32_t>: SqlSimpleDataBinder<uint32_t, SQL_C_ULONG, SQL_INTEGER, SqlColumnType::INTEGER> {};
template <> struct SqlDataBinder<int64_t>: SqlSimpleDataBinder<int64_t, SQL_C_SBIGINT, SQL_BIGINT, SqlColumnType::BIGINT> {};
template <> struct SqlDataBinder<uint64_t>: SqlSimpleDataBinder<uint64_t, SQL_C_UBIGINT, SQL_BIGINT, SqlColumnType::BIGINT> {};
template <> struct SqlDataBinder<float>: SqlSimpleDataBinder<float, SQL_C_FLOAT, SQL_REAL, SqlColumnType::REAL> {};
template <> struct SqlDataBinder<double>: SqlSimpleDataBinder<double, SQL_C_DOUBLE, SQL_DOUBLE, SqlColumnType::REAL> {};
#if !defined(_WIN32) && !defined(__APPLE__)
template <> struct SqlDataBinder<long long>: SqlSimpleDataBinder<long long, SQL_C_SBIGINT, SQL_BIGINT, SqlColumnType::BIGINT> {};
template <> struct SqlDataBinder<unsigned long long>: SqlSimpleDataBinder<unsigned long long, SQL_C_UBIGINT, SQL_BIGINT, SqlColumnType::BIGINT> {};
#endif
#if defined(__APPLE__) // size_t is a different type on macOS
template <> struct SqlDataBinder<std::size_t>: SqlSimpleDataBinder<std::size_t, SQL_C_SBIGINT, SQL_BIGINT> {};
#endif
// clang-format on
