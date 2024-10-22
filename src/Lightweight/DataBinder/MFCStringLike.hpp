#pragma once

#include "Core.hpp"

#include <concepts>

template <typename T>
concept MFCStringLike = requires(T const& t) {
    { t.GetLength() } -> std::same_as<int>;
    { t.GetString() } -> std::same_as<char const*>;
};

template <typename T>
    requires(MFCStringLike<T>)
struct SqlDataBinder<T>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, T const& value) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                value.GetLength(),
                                0,
                                (SQLPOINTER) value.GetString(),
                                0,
                                nullptr);
    }
};

// TODO: Use the below instead in order to get full support for MFC-like strings.

// template <typename T>
//     requires(MFCStringLike<T>)
// struct SqlOutputStringTraits<T>
// {
//     static char const* Data(MFCStringLike auto const* str) noexcept
//     {
//         return str->GetString();
//     }
//
//     static char* Data(MFCStringLike auto* str) noexcept
//     {
//         return str->GetString();
//     }
//
//     static SQLULEN Size(MFCStringLike auto const* str) noexcept
//     {
//         return static_cast<SQLULEN>(str->GetLength());
//     }
//
//     static void Clear(MFCStringLike auto* str) noexcept
//     {
//         *str = {};
//     }
//
//     static void Reserve(MFCStringLike auto* str, size_t capacity) noexcept
//     {
//         // TODO(pr)
//         // str->reserve(capacity);
//         // str->resize(str->capacity());
//     }
//
//     static void Resize(MFCStringLike auto* str, SQLLEN indicator) noexcept
//     {
//         // TODO(pr)
//         // if (indicator > 0)
//         //     str->resize(indicator);
//     }
// };