// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../SqlLogger.hpp"
#include "MFCStringLike.hpp"
#include "Primitives.hpp"
#include "SqlDate.hpp"
#include "SqlDateTime.hpp"
#include "SqlNullValue.hpp"
#include "SqlText.hpp"
#include "SqlTime.hpp"
#include "StdString.hpp"
#include "StdStringView.hpp"

#include <format>
#include <print>
#include <variant>

template <typename>
struct SqlViewHelper;

namespace detail
{
template <class... Ts>
struct overloaded: Ts... // NOLINT(readability-identifier-naming)
{
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// clang-format off
template <typename T>
concept HasGetStringAndGetLength = requires(T const& t) {
    { t.GetLength() } -> std::same_as<int>;
    { t.GetString() } -> std::same_as<char const*>;
};

template <typename T>
concept HasGetStringAndLength = requires(T const& t)
{
    { t.Length() } -> std::same_as<int>;
    { t.GetString() } -> std::same_as<char const*>;
};

template <typename T>
concept HasSqlViewHelper = requires(T const& t)
{
    { SqlViewHelper<T>::GetView(t) } -> std::convertible_to<std::string_view>;
};
// clang-format on

} // namespace detail

template <>
struct SqlViewHelper<std::string>
{
    static std::string_view GetView(std::string const& str) noexcept
    {
        return { str.data(), str.size() };
    }
};

template <detail::HasGetStringAndGetLength CStringLike>
struct SqlViewHelper<CStringLike>
{
    static std::string_view GetView(CStringLike const& str) noexcept
    {
        return { str.GetString(), static_cast<size_t>(str.GetLength()) };
    }
};

template <detail::HasGetStringAndLength StringLike>
struct SqlViewHelper<StringLike>
{
    static std::string_view GetView(StringLike const& str) noexcept
    {
        return { str.GetString(), static_cast<size_t>(str.Length()) };
    }
};

struct SqlVariant
{
    using InnerType = std::variant<SqlNullType,
                                   bool,
                                   short,
                                   unsigned short,
                                   int,
                                   unsigned int,
                                   long long,
                                   unsigned long long,
                                   float,
                                   double,
                                   std::string,
                                   std::string_view,
                                   SqlText,
                                   SqlDate,
                                   SqlTime,
                                   SqlDateTime>;

    InnerType value;

    SqlVariant() = default;
    SqlVariant(SqlVariant const&) = default;
    SqlVariant(SqlVariant&&) noexcept = default;
    SqlVariant& operator=(SqlVariant const&) = default;
    SqlVariant& operator=(SqlVariant&&) noexcept = default;
    ~SqlVariant() = default;

    LIGHTWEIGHT_FORCE_INLINE SqlVariant(InnerType const& other):
        value(other)
    {
    }

    LIGHTWEIGHT_FORCE_INLINE SqlVariant(InnerType&& other) noexcept:
        value(std::move(other))
    {
    }

    LIGHTWEIGHT_FORCE_INLINE SqlVariant& operator=(InnerType const& other)
    {
        value = other;
        return *this;
    }

    LIGHTWEIGHT_FORCE_INLINE SqlVariant& operator=(InnerType&& other) noexcept
    {
        value = std::move(other);
        return *this;
    }

    // Construct from an string-like object that implements an SqlViewHelper<>.
    template <detail::HasSqlViewHelper StringViewLike>
    LIGHTWEIGHT_FORCE_INLINE explicit SqlVariant(StringViewLike const* newValue):
        value { SqlViewHelper<std::remove_cv_t<decltype(*newValue)>>::GetView(*newValue) }
    {
    }

    // Assign from an string-like object that implements an SqlViewHelper<>.
    template <detail::HasSqlViewHelper StringViewLike>
    LIGHTWEIGHT_FORCE_INLINE SqlVariant& operator=(StringViewLike const* newValue) noexcept
    {
        value = std::string_view(newValue->GetString(), newValue->GetLength());
        return *this;
    }

    // Check if the value is NULL.
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE bool IsNull() const noexcept
    {
        return std::holds_alternative<SqlNullType>(value);
    }

    // Check if the value is of the specified type.
    template <typename T>
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE bool Is() const noexcept
    {
        return std::holds_alternative<T>(value);
    }

    // Retrieve the value as the specified type.
    template <typename T>
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE T& Get() noexcept
    {
        return std::get<T>(value);
    }

    // Retrieve the value as the specified type, or return the default value if the value is NULL.
    template <typename T>
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE T ValueOr(T&& defaultValue) const noexcept
    {
        if constexpr (std::is_integral_v<T>)
            return TryGetIntegral<T>().value_or(std::forward<T>(defaultValue));

        if (IsNull())
            return std::forward<T>(defaultValue);

        return std::get<T>(value);
    }

    // clang-format off
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<bool> TryGetBool() const noexcept { return TryGetIntegral<bool>(); }
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<short> TryGetShort() const noexcept { return TryGetIntegral<short>(); }
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<unsigned short> TryGetUShort() const noexcept { return TryGetIntegral<unsigned short>(); }
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<int> TryGetInt() const noexcept { return TryGetIntegral<int>(); }
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<unsigned int> TryGetUInt() const noexcept { return TryGetIntegral<unsigned int>(); }
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<long long> TryGetLongLong() const noexcept { return TryGetIntegral<long long>(); }
    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<unsigned long long> TryGetULongLong() const noexcept { return TryGetIntegral<unsigned long long>(); }
    // clang-format on

    template <typename ResultType>
    [[nodiscard]] std::optional<ResultType> TryGetIntegral() const noexcept
    {
        if (IsNull())
            return std::nullopt;

        // clang-format off
        return std::visit(detail::overloaded {
            []<typename T>(T v) -> ResultType requires(std::is_integral_v<T>) { return static_cast<ResultType>(v); },
            [](auto) -> ResultType { throw std::bad_variant_access(); }
        }, value);
        // clang-format on
    }

    [[nodiscard]] std::optional<std::string_view> TryGetStringView() const noexcept
    {
        if (IsNull())
            return std::nullopt;

        // clang-format off
        return std::visit(detail::overloaded {
            [](std::string_view v) { return v; },
            [](std::string const& v) { return std::string_view(v.data(), v.size()); },
            [](SqlText const& v) { return std::string_view(v.value.data(), v.value.size()); },
            [](auto) -> std::string_view { throw std::bad_variant_access(); }
        }, value);
        // clang-format on
    }

    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<SqlDate> TryGetDate() const
    {
        if (IsNull())
            return std::nullopt;

        if (auto const* date = std::get_if<SqlDate>(&value))
            return *date;

        throw std::bad_variant_access();
    }

    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<SqlTime> TryGetTime() const
    {
        if (IsNull())
            return std::nullopt;

        if (auto const* time = std::get_if<SqlTime>(&value))
            return *time;

        throw std::bad_variant_access();
    }

    [[nodiscard]] LIGHTWEIGHT_FORCE_INLINE std::optional<SqlDateTime> TryGetDateTime() const
    {
        if (IsNull())
            return std::nullopt;

        if (auto const* dateTime = std::get_if<SqlDateTime>(&value))
            return *dateTime;

        throw std::bad_variant_access();
    }
};

template <>
struct LIGHTWEIGHT_API SqlDataBinder<SqlVariant>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt,
                                    SQLUSMALLINT column,
                                    SqlVariant const& variantValue,
                                    SqlDataBinderCallback& cb) noexcept;

    static SQLRETURN GetColumn(SQLHSTMT stmt,
                               SQLUSMALLINT column,
                               SqlVariant* result,
                               SQLLEN* indicator,
                               SqlDataBinderCallback const& cb) noexcept;
};
