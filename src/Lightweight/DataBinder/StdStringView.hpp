// SPDX-License-Identifier: MIT
#pragma once

#include "Core.hpp"

#include <string_view>

template <>
struct SqlDataBinder<std::string_view>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, std::string_view value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                value.size(),
                                0,
                                (SQLPOINTER) value.data(),
                                0,
                                nullptr);
    }
};
