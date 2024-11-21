// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <format>
#include <string>
#include <type_traits>

namespace detail
{

template <typename T, typename... Comps>
concept OneOf = (std::same_as<T, Comps> || ...);

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
        if constexpr (OneOf<T, std::string, std::string_view, char const*>)
            output += value;
        else
            output += std::format("{}", std::forward<T>(value));
        return *this;
    }
};

} // namespace detail
