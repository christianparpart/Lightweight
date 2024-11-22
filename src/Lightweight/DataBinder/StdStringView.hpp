// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"
#include "UnicodeConverter.hpp"

#include <concepts>
#include <format>
#include <string_view>

template <typename CharT>
    requires(std::same_as<CharT, char> || std::same_as<CharT, char16_t>
             || (std::same_as<CharT, wchar_t> && sizeof(wchar_t) == 2))
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

    static LIGHTWEIGHT_FORCE_INLINE std::string_view Inspect(std::basic_string_view<CharT> value) noexcept
        requires(std::same_as<CharT, char>)
    {
        return { value.data(), value.size() };
    }

    static LIGHTWEIGHT_FORCE_INLINE std::string Inspect(std::basic_string_view<CharT> value) noexcept
        requires(!std::same_as<CharT, char>)
    {
        auto u8String = ToUtf8(value);
        return std::string((char const*) u8String.data(), u8String.size());
    }
};
