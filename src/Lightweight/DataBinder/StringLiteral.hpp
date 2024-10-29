// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

#include <concepts>

template <std::size_t N>
struct SqlDataBinder<char[N]>
{
    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt,
                                                             SQLUSMALLINT column,
                                                             char const* value,
                                                             SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindParameter(
            stmt, column, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, N - 1, 0, (SQLPOINTER) value, 0, nullptr);
    }
};

template <typename T, std::size_t N>
    requires(std::same_as<T, char16_t> || (std::same_as<T, wchar_t> && sizeof(wchar_t) == 2))
struct SqlDataBinder<T[N]>
{
    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt,
                                    SQLUSMALLINT column,
                                    T const* value,
                                    SqlDataBinderCallback& /*cb*/) noexcept
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
