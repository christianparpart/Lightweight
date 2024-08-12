#pragma once

#include "../SqlDataBinder.hpp"
#include "RecordId.hpp"

#include <string_view>
#include <string>
#include <cstdint>

namespace Model
{

enum class ColumnType : uint8_t
{
    UNKNOWN,
    STRING,
    BOOLEAN,
    INTEGER,
    REAL,
    BLOB,
    DATE,
    TIME,
    TIMESTAMP,
};

constexpr std::string_view ColumnTypeName(ColumnType value) noexcept
{
    switch (value)
    {
        case ColumnType::STRING:
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
        case ColumnType::TIMESTAMP:
            return "TIMESTAMP";
    }
    return "UNKNOWN";
}

namespace detail
{
template <typename>
struct ColumnTypeOf;

// clang-format off
template <> struct ColumnTypeOf<std::string> { static constexpr ColumnType value = ColumnType::STRING; };
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
template <> struct ColumnTypeOf<SqlTimestamp> { static constexpr ColumnType value = ColumnType::TIMESTAMP; };
template <> struct ColumnTypeOf<RecordId> { static constexpr ColumnType value = ColumnType::INTEGER; };
// clang-format on
} // namespace detail

template <typename T>
constexpr ColumnType ColumnTypeOf = detail::ColumnTypeOf<T>::value;

} // namespace Model
