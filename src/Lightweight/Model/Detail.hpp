// SPDX-License-Identifier: MIT
#pragma once

#include <format>
#include <string>
#include <type_traits>

namespace Model::detail
{

struct StringBuilder
{
    std::string output;

    std::string operator*() const& noexcept
    {
        return output;
    }

    std::string operator*() && noexcept
    {
        return std::move(output);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return output.empty();
    }

    template <typename T>
    StringBuilder& operator<<(T&& value)
    {
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>
                      || std::is_same_v<T, char const*>)
            output += value;
        else
            output += std::format("{}", std::forward<T>(value));
        return *this;
    }
};

} // namespace Model::detail
