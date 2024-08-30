// SPDX-License-Identifier: MIT
#pragma once

#include "../SqlDataBinder.hpp"
#include "../SqlTraits.hpp"
#include "RecordId.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace Model
{

namespace detail
{
    template <typename>
    struct ColumnTypeOf;

    // clang-format off
template <> struct ColumnTypeOf<char> { static constexpr SqlColumnType value = SqlColumnType::CHAR; };
template <std::size_t N, typename T, SqlStringPostRetrieveOperation P> struct ColumnTypeOf<SqlFixedString<N, T, P>> { static constexpr SqlColumnType value = SqlColumnType::STRING; };
template <> struct ColumnTypeOf<std::string> { static constexpr SqlColumnType value = SqlColumnType::STRING; };
template <> struct ColumnTypeOf<SqlTrimmedString> { static constexpr SqlColumnType value = SqlColumnType::STRING; };
template <> struct ColumnTypeOf<SqlText> { static constexpr SqlColumnType value = SqlColumnType::TEXT; };
template <> struct ColumnTypeOf<bool> { static constexpr SqlColumnType value = SqlColumnType::BOOLEAN; };
template <> struct ColumnTypeOf<int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct ColumnTypeOf<unsigned int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct ColumnTypeOf<long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct ColumnTypeOf<unsigned long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct ColumnTypeOf<long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct ColumnTypeOf<unsigned long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct ColumnTypeOf<float> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct ColumnTypeOf<double> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct ColumnTypeOf<SqlDate> { static constexpr SqlColumnType value = SqlColumnType::DATE; };
template <> struct ColumnTypeOf<SqlTime> { static constexpr SqlColumnType value = SqlColumnType::TIME; };
template <> struct ColumnTypeOf<SqlDateTime> { static constexpr SqlColumnType value = SqlColumnType::DATETIME; };
template <> struct ColumnTypeOf<RecordId> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
    // clang-format on
} // namespace detail

template <typename T>
constexpr SqlColumnType ColumnTypeOf = detail::ColumnTypeOf<T>::value;

} // namespace Model
