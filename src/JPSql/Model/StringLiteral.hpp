#pragma once

#include <algorithm>

namespace Model
{

template <size_t N>
struct SqlStringLiteral
{
    constexpr SqlStringLiteral(const char (&str)[N]) noexcept
    {
        std::copy_n(str, N, value);
    }

    constexpr std::string_view operator*() const noexcept
    {
        return value;
    }

    char value[N];
};

} // namespace Model
