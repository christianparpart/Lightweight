// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"
#include "StdString.hpp"

#include <compare>
#include <format>
#include <string>
#include <utility>

// Helper struct to store a string that should be automatically trimmed when fetched from the database.
// This is only needed for compatibility with old columns that hard-code the length, like CHAR(50).
struct SqlTrimmedString
{
    std::string value;

    std::weak_ordering operator<=>(SqlTrimmedString const&) const noexcept = default;
};

template <>
struct std::formatter<SqlTrimmedString>: std::formatter<std::string>
{
    LIGHTWEIGHT_API auto format(SqlTrimmedString const& text, format_context& ctx) const -> format_context::iterator
    {
        return std::formatter<std::string>::format(text.value, ctx);
    }
};

template <>
struct SqlDataBinder<SqlTrimmedString>
{
    static constexpr auto ColumnType = SqlColumnType::STRING;

    using InnerStringType = decltype(std::declval<SqlTrimmedString>().value);
    using StringTraits = SqlBasicStringOperations<InnerStringType>;

    static void TrimRight(InnerStringType* boundOutputString, SQLLEN indicator) noexcept
    {
        if (indicator < 0) // e.g. SQL_NULL_DATA
            return;
        size_t n = indicator;
        while (n > 0 && std::isspace((*boundOutputString)[n - 1]))
            --n;
        StringTraits::Resize(boundOutputString, static_cast<SQLLEN>(n));
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt,
                                                             SQLUSMALLINT column,
                                                             SqlTrimmedString const& value,
                                                             SqlDataBinderCallback& cb) noexcept
    {
        return SqlDataBinder<InnerStringType>::InputParameter(stmt, column, value.value, cb);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN OutputColumn(SQLHSTMT stmt,
                                                           SQLUSMALLINT column,
                                                           SqlTrimmedString* result,
                                                           SQLLEN* indicator,
                                                           SqlDataBinderCallback& cb) noexcept
    {
        auto* boundOutputString = &result->value;
        cb.PlanPostProcessOutputColumn([indicator, boundOutputString]() {
            // NB: If the indicator is greater than the buffer size, we have a truncation.
            auto const bufferSize = StringTraits::Size(boundOutputString);
            auto const len = std::cmp_greater_equal(*indicator, bufferSize) || *indicator == SQL_NO_TOTAL
                                 ? bufferSize - 1
                                 : *indicator;
            TrimRight(boundOutputString, static_cast<SQLLEN>(len));
        });
        return SQLBindCol(stmt,
                          column,
                          SQL_C_CHAR,
                          (SQLPOINTER) StringTraits::Data(boundOutputString),
                          (SQLLEN) StringTraits::Size(boundOutputString),
                          indicator);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN GetColumn(SQLHSTMT stmt,
                                                        SQLUSMALLINT column,
                                                        SqlTrimmedString* result,
                                                        SQLLEN* indicator,
                                                        SqlDataBinderCallback const& cb) noexcept
    {
        auto const returnCode = SqlDataBinder<InnerStringType>::GetColumn(stmt, column, &result->value, indicator, cb);
        TrimRight(&result->value, *indicator);
        return returnCode;
    }

    static LIGHTWEIGHT_FORCE_INLINE std::string_view Inspect(SqlTrimmedString const& value) noexcept
    {
        return { value.value.data(), static_cast<size_t>(value.value.size()) };
    }
};
