// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

template <std::size_t N>
struct SqlDataBinder<char[N]>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, char const* value) noexcept
    {
        return SQLBindParameter(
            stmt, column, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, N - 1, 0, (SQLPOINTER) value, 0, nullptr);
    }
};

template <typename T, std::size_t N>
    requires(std::is_same_v<T, char16_t> || (std::is_same_v<T, wchar_t> && sizeof(wchar_t) == 2))
struct SqlDataBinder<T[N]>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, T const* value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_WCHAR,
                                SQL_WVARCHAR,
                                (N - 1) * sizeof(T),
                                0,
                                (SQLPOINTER) value,
                                0,
                                nullptr);
    }
};
