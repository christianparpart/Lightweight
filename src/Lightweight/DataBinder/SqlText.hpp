// SPDX-License-Identifier: MIT
#pragma once

#include "Core.hpp"
#include "StdString.hpp"

#include <string>

// Represents a TEXT field in a SQL database.
//
// This is used for large texts, e.g. up to 65k characters.
struct SqlText
{
    using value_type = std::string;

    value_type value;

    std::weak_ordering operator<=>(SqlText const&) const noexcept = default;
};

template <>
struct std::formatter<SqlText>: std::formatter<std::string>
{
    auto format(SqlText const& text, format_context& ctx) const -> format_context::iterator
    {
        return std::formatter<std::string>::format(text.value, ctx);
    }
};

template <>
struct SqlOutputStringTraits<SqlText>
{
    using Traits = SqlOutputStringTraits<typename SqlText::value_type>;

    // clang-format off
    static char const* Data(SqlText const* str) noexcept { return Traits::Data(&str->value); }
    static char* Data(SqlText* str) noexcept { return Traits::Data(&str->value); }
    static SQLULEN Size(SqlText const* str) noexcept { return Traits::Size(&str->value); }
    static void Clear(SqlText* str) noexcept { Traits::Clear(&str->value); }
    static void Reserve(SqlText* str, size_t capacity) noexcept { Traits::Reserve(&str->value, capacity); }
    static void Resize(SqlText* str, SQLLEN indicator) noexcept { Traits::Resize(&str->value, indicator); }
    // clang-format on
};
