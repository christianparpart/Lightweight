// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"
#include "UnicodeConverter.hpp"

#include <concepts>
#include <cstdlib>
#include <string>
#include <type_traits>

// Specialized traits for std::basic_string<> as output string parameter
template <typename CharT>
struct SqlBasicStringOperations<std::basic_string<CharT>>
{
    using CharType = CharT;
    using StringType = std::basic_string<CharT>;

    static CharType const* Data(StringType const* str) noexcept
    {
        return str->data();
    }

    static CharType* Data(StringType* str) noexcept
    {
        return str->data();
    }

    static SQLULEN Size(StringType const* str) noexcept
    {
        return str->size();
    }

    static void Clear(StringType* str) noexcept
    {
        str->clear();
    }

    static void Reserve(StringType* str, size_t capacity) noexcept
    {
        // std::basic_string<> tries to defer the allocation as long as possible.
        // So we first tell StringType how much to reserve and then resize it to the *actually* reserved
        // size.
        str->reserve(capacity);
        str->resize(str->capacity());
    }

    static void Resize(StringType* str, SQLLEN indicator) noexcept
    {
        if (indicator > 0)
            str->resize(indicator);
    }
};

template <>
struct SqlDataTraits<std::string>
{
    static constexpr unsigned Size = 0;
    static constexpr SqlColumnType Type = SqlColumnType::STRING;
};
