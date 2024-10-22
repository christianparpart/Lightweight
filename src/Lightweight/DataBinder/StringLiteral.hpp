// SPDX-License-Identifier: MIT

#pragma once

#include "Core.hpp"

template <std::size_t N>
struct SqlDataBinder<char[N]>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, char const* value) noexcept
    {
        static_assert(N > 0, "N must be greater than 0"); // I cannot imagine that N is 0, ever.
        return SQLBindParameter(
            stmt, column, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, N - 1, 0, (SQLPOINTER) value, 0, nullptr);
    }
};
