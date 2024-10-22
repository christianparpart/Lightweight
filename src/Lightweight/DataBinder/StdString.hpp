// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

#include <concepts>
#include <cstdlib>
#include <string>
#include <type_traits>

namespace detail
{

template <typename>
struct UnicodeConverter;

template <>
struct UnicodeConverter<char16_t>
{
    // Converts a UTF-32 code point to one or two UTF-16 code units.
    template <typename OutputIterator>
    constexpr OutputIterator Convert(char32_t input, OutputIterator output) noexcept
    {
        if (input < 0xD800) // [0x0000 .. 0xD7FF]
        {
            *output++ = char16_t(input);
            return output;
        }
        else if (input < 0x10000)
        {
            if (input < 0xE000)
                return output; // The UTF-16 code point can not be in surrogate range.

            // [0xE000 .. 0xFFFF]
            *output++ = char16_t(input);
            return output;
        }
        else if (input < 0x110000) // [0xD800 .. 0xDBFF] [0xDC00 .. 0xDFFF]
        {
            *output++ = char16_t(0xD7C0 + (input >> 10));
            *output++ = char16_t(0xDC00 + (input & 0x3FF));
            return output;
        }
        else
            return output; // Too large UTF-16 code point.
    }
};

template <typename T>
    requires std::is_same_v<T, char32_t> || (std::is_same_v<T, wchar_t> && sizeof(wchar_t) == 4)
std::u16string ToUtf16(const std::basic_string_view<T> u32String)
{
    std::u16string u16String;
    u16String.reserve(u32String.size());
    UnicodeConverter<char16_t> converter;
    for (auto const c: u32String)
        converter.Convert(c, std::back_inserter(u16String));
    return u16String;
}

} // namespace detail

// Specialized traits for std::basic_string<> as output string parameter
template <typename CharT>
struct SqlCommonStringBinder<std::basic_string<CharT>>
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
    static constexpr inline unsigned Size = 0;
    static constexpr inline SqlColumnType Type = SqlColumnType::STRING;
};
