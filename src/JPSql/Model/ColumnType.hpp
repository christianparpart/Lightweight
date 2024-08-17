// SPDX-License-Identifier: MIT
#pragma once

#include "../SqlDataBinder.hpp"
#include "RecordId.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace Model
{

enum class ColumnType : uint8_t
{
    UNKNOWN,
    STRING,
    TEXT,
    BOOLEAN,
    INTEGER,
    REAL,
    BLOB,
    DATE,
    TIME,
    DATETIME,
    TIMESTAMP,
};

constexpr std::string_view ColumnTypeName(ColumnType value) noexcept
{
    switch (value)
    {
        case ColumnType::STRING:
            return "VARCHAR";
        case ColumnType::TEXT:
            return "TEXT";
        case ColumnType::BOOLEAN:
            return "BOOLEAN";
        case ColumnType::INTEGER:
            return "INTEGER";
        case ColumnType::REAL:
            return "REAL";
        case ColumnType::BLOB:
            return "BLOB";
        case ColumnType::DATE:
            return "DATE";
        case ColumnType::TIME:
            return "TIME";
        case ColumnType::DATETIME:
            return "DATETIME";
        case ColumnType::TIMESTAMP:
            return "TIMESTAMP";
        case ColumnType::UNKNOWN:
            break;
    }
    return "UNKNOWN";
}

namespace detail
{
    template <typename>
    struct ColumnTypeOf;

    // clang-format off
template <> struct ColumnTypeOf<std::string> { static constexpr ColumnType value = ColumnType::STRING; };
template <> struct ColumnTypeOf<SqlText> { static constexpr ColumnType value = ColumnType::TEXT; };
template <> struct ColumnTypeOf<bool> { static constexpr ColumnType value = ColumnType::BOOLEAN; };
template <> struct ColumnTypeOf<int> { static constexpr ColumnType value = ColumnType::INTEGER; };
template <> struct ColumnTypeOf<unsigned int> { static constexpr ColumnType value = ColumnType::INTEGER; };
template <> struct ColumnTypeOf<long> { static constexpr ColumnType value = ColumnType::INTEGER; };
template <> struct ColumnTypeOf<unsigned long> { static constexpr ColumnType value = ColumnType::INTEGER; };
template <> struct ColumnTypeOf<long long> { static constexpr ColumnType value = ColumnType::INTEGER; };
template <> struct ColumnTypeOf<unsigned long long> { static constexpr ColumnType value = ColumnType::INTEGER; };
template <> struct ColumnTypeOf<float> { static constexpr ColumnType value = ColumnType::REAL; };
template <> struct ColumnTypeOf<double> { static constexpr ColumnType value = ColumnType::REAL; };
template <> struct ColumnTypeOf<SqlDate> { static constexpr ColumnType value = ColumnType::DATE; };
template <> struct ColumnTypeOf<SqlTime> { static constexpr ColumnType value = ColumnType::TIME; };
template <> struct ColumnTypeOf<SqlDateTime> { static constexpr ColumnType value = ColumnType::DATETIME; };
template <> struct ColumnTypeOf<SqlTimestamp> { static constexpr ColumnType value = ColumnType::TIMESTAMP; };
template <> struct ColumnTypeOf<RecordId> { static constexpr ColumnType value = ColumnType::INTEGER; };
    // clang-format on
} // namespace detail

template <typename T>
constexpr ColumnType ColumnTypeOf = detail::ColumnTypeOf<T>::value;

} // namespace Model
