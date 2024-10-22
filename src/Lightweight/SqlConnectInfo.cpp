// SPDX-License-Identifier: Apache-2.0

#include "SqlConnectInfo.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>

namespace
{

constexpr std::string_view DropQuotation(std::string_view value) noexcept
{
    if (!value.empty() && value.front() == '{' && value.back() == '}')
    {
        value.remove_prefix(1);
        value.remove_suffix(1);
    }
    return value;
}

constexpr std::string_view Trim(std::string_view value) noexcept
{
    while (!value.empty() && std::isspace(value.front()))
        value.remove_prefix(1);

    while (!value.empty() && std::isspace(value.back()))
        value.remove_suffix(1);

    return value;
}

std::string ToUpperCaseString(std::string_view input)
{
    std::string result { input };
    std::ranges::transform(result, result.begin(), [](char c) { return (char) std::toupper(c); });
    return result;
}

} // end namespace

SqlConnectionStringMap ParseConnectionString(SqlConnectionString const& connectionString)
{
    auto pairs = connectionString.value | std::views::split(';') | std::views::transform([](auto pair_view) {
                     return std::string_view(&*pair_view.begin(), std::ranges::distance(pair_view));
                 });

    SqlConnectionStringMap result;

    for (auto const& pair: pairs)
    {
        auto separatorPosition = pair.find('=');
        if (separatorPosition != std::string_view::npos)
        {
            auto const key = Trim(pair.substr(0, separatorPosition));
            auto const value = DropQuotation(Trim(pair.substr(separatorPosition + 1)));
            result.insert_or_assign(ToUpperCaseString(key), std::string(value));
        }
    }

    return result;
}

SqlConnectionString BuildConnectionString(SqlConnectionStringMap const& map)
{
    SqlConnectionString result;

    for (auto const& [key, value]: map)
    {
        std::string_view const delimiter = result.value.empty() ? "" : ";";
        result.value += std::format("{}{}={{{}}}", delimiter, key, value);
    }

    return result;
}
