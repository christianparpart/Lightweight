// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <iterator>
#include <string>
#include <string_view>

namespace detail
{

template <typename>
struct UnicodeConverter;

template <>
struct UnicodeConverter<char8_t>
{
    // Converts a UTF-32 code point to one to four UTF-8 code units.
    template <typename OutputIterator>
    constexpr OutputIterator Convert(char32_t input, OutputIterator output) noexcept
    {
        if (input <= 0x7F)
        {
            *output++ = static_cast<char8_t>(input & 0b0111'1111);
        }
        else if (input <= 0x07FF)
        {
            *output++ = static_cast<char8_t>(((input >> 6) & 0b0001'1111) | 0b1100'0000);
            *output++ = static_cast<char8_t>(((input >> 0) & 0b0011'1111) | 0b1000'0000);
        }
        else if (input <= 0xFFFF)
        {
            *output++ = static_cast<char8_t>(((input >> 12) & 0b0000'1111) | 0b1110'0000);
            *output++ = static_cast<char8_t>(((input >> 6) & 0b0011'1111) | 0b1000'0000);
            *output++ = static_cast<char8_t>(((input >> 0) & 0b0011'1111) | 0b1000'0000);
        }
        else
        {
            *output++ = static_cast<char8_t>(((input >> 18) & 0b0000'0111) | 0b1111'0000);
            *output++ = static_cast<char8_t>(((input >> 12) & 0b0011'1111) | 0b1000'0000);
            *output++ = static_cast<char8_t>(((input >> 6) & 0b0011'1111) | 0b1000'0000);
            *output++ = static_cast<char8_t>(((input >> 0) & 0b0011'1111) | 0b1000'0000);
        }
        return output;
    }
};

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

} // namespace detail

// Converts from UTF-32 to UTF-8.
std::u8string ToUtf8(std::u32string_view u32InputString);

// Converts from UTF-16 to UTF-8.
std::u8string ToUtf8(std::u16string_view u16InputString);

// Converts from UTF-32 to UTF-16.
template <typename T>
    requires std::is_same_v<T, char32_t> || (std::is_same_v<T, wchar_t> && sizeof(wchar_t) == 4)
std::u16string ToUtf16(const std::basic_string_view<T> u32InputString)
{
    std::u16string u16OutputString;
    u16OutputString.reserve(u32InputString.size());
    detail::UnicodeConverter<char16_t> converter;
    for (auto const c: u32InputString)
        converter.Convert(c, std::back_inserter(u16OutputString));
    return u16OutputString;
}

// Converts from UTF-8 to UTF-16.
std::u16string ToUtf16(std::u8string_view u8InputString);
