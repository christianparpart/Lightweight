// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

#include <string_view>

template <typename CharT>
    requires(std::is_same_v<CharT, char> || std::is_same_v<CharT, char16_t>
             || (std::is_same_v<CharT, wchar_t> && sizeof(wchar_t) == 2))
struct SqlDataBinder<std::basic_string_view<CharT>>
{
    static constexpr SQLSMALLINT cType = sizeof(CharT) == 1 ? SQL_C_CHAR : SQL_C_WCHAR;
    static constexpr SQLSMALLINT sqlType = sizeof(CharT) == 1 ? SQL_VARCHAR : SQL_WVARCHAR;

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt,
                                                             SQLUSMALLINT column,
                                                             std::basic_string_view<CharT> value,
                                                             SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindParameter(
            stmt, column, SQL_PARAM_INPUT, cType, sqlType, value.size(), 0, (SQLPOINTER) value.data(), 0, nullptr);
    }
};
