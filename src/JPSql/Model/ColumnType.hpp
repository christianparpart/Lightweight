#pragma once

#include "../SqlDataBinder.hpp"
#include "ModelId.hpp"

#include <string_view>
#include <string>
#include <cstdint>

namespace Model
{

enum class SqlColumnType : uint8_t
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

constexpr std::string_view SqlColumnTypeName(SqlColumnType value) noexcept
{
    switch (value)
    {
        case SqlColumnType::STRING:
            return "TEXT";
        case SqlColumnType::BOOLEAN:
            return "BOOLEAN";
        case SqlColumnType::INTEGER:
            return "INTEGER";
        case SqlColumnType::REAL:
            return "REAL";
        case SqlColumnType::BLOB:
            return "BLOB";
        case SqlColumnType::DATE:
            return "DATE";
        case SqlColumnType::TIME:
            return "TIME";
        case SqlColumnType::TIMESTAMP:
            return "TIMESTAMP";
    }
    return "UNKNOWN";
}

namespace detail
{
template <typename>
struct SqlColumnTypeOf;

// clang-format off
template <> struct SqlColumnTypeOf<std::string> { static constexpr SqlColumnType value = SqlColumnType::STRING; };
template <> struct SqlColumnTypeOf<bool> { static constexpr SqlColumnType value = SqlColumnType::BOOLEAN; };
template <> struct SqlColumnTypeOf<int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<float> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct SqlColumnTypeOf<double> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct SqlColumnTypeOf<SqlDate> { static constexpr SqlColumnType value = SqlColumnType::DATE; };
template <> struct SqlColumnTypeOf<SqlTime> { static constexpr SqlColumnType value = SqlColumnType::TIME; };
template <> struct SqlColumnTypeOf<SqlTimestamp> { static constexpr SqlColumnType value = SqlColumnType::TIMESTAMP; };
template <> struct SqlColumnTypeOf<SqlModelId> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
// clang-format on
} // namespace detail

template <typename T>
constexpr SqlColumnType SqlColumnTypeOf = detail::SqlColumnTypeOf<T>::value;

} // namespace Model
