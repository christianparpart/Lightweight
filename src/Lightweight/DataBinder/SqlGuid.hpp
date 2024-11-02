// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BasicStringBinder.hpp"
#include "Core.hpp"
#include "StdString.hpp"

#include <format>
#include <optional>
#include <string>

struct LIGHTWEIGHT_API SqlGuid
{
    uint8_t data[16] {};

    static SqlGuid Create() noexcept;

    static std::optional<SqlGuid> TryParse(std::string_view const& text) noexcept;

    constexpr std::weak_ordering operator<=>(SqlGuid const& other) const noexcept = default;
};

template <>
struct LIGHTWEIGHT_API std::formatter<SqlGuid>: std::formatter<std::string>
{
    auto format(SqlGuid const& guid, format_context& ctx) const -> format_context::iterator
    {
        // clang-format off
        return formatter<std::string>::format(std::format(
                "{:08X}-{:04X}-{:04X}-{:04X}-{:012X}",
                (uint32_t) guid.data[3] | (uint32_t) guid.data[2] << 8 |
                    (uint32_t) guid.data[1] << 16 | (uint32_t) guid.data[0] << 24,
                (uint16_t) guid.data[5] | (uint16_t) guid.data[4] << 8,
                (uint16_t) guid.data[7] | (uint16_t) guid.data[6] << 8,
                (uint16_t) guid.data[8] | (uint16_t) guid.data[9] << 8,
                (uint64_t) guid.data[15] | (uint64_t) guid.data[14] << 8 |
                    (uint64_t) guid.data[13] << 16 | (uint64_t) guid.data[12] << 24 |
                    (uint64_t) guid.data[11] << 32 | (uint64_t) guid.data[10] << 40
            ),
            ctx
        );
        // clang-format on
    }
};

inline LIGHTWEIGHT_FORCE_INLINE std::string to_string(SqlGuid const& guid)
{
    return std::format("{}", guid);
}

template <>
struct LIGHTWEIGHT_API SqlDataBinder<SqlGuid>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt,
                                    SQLUSMALLINT column,
                                    SqlGuid const& value,
                                    SqlDataBinderCallback& cb) noexcept;

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, SqlGuid* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept;

    static SQLRETURN GetColumn(SQLHSTMT stmt,
                               SQLUSMALLINT column,
                               SqlGuid* result,
                               SQLLEN* indicator,
                               SqlDataBinderCallback const& cb) noexcept;
};
