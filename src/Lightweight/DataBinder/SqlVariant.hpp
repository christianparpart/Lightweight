// SPDX-License-Identifier: MIT
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

namespace detail
{
template <class... Ts>
struct overloaded: Ts... // NOLINT(readability-identifier-naming)
{
    using Ts::operator()...;
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
} // namespace detail

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

    SqlVariant(InnerType const& other):
        value(other)
    {
    }

    SqlVariant(InnerType&& other) noexcept:
        value(std::move(other))
    {
    }

    SqlVariant& operator=(InnerType const& other)
    {
        value = other;
        return *this;
    }

    SqlVariant& operator=(InnerType&& other) noexcept
    {
        value = std::move(other);
        return *this;
    }

    // Construct from an MFC-string-like object.
    explicit SqlVariant(MFCStringLike auto* newValue):
        value { std::string_view(newValue->GetString(), newValue->GetLength()) }
    {
    }

    // Assign from an MFC-string-like object.
    SqlVariant& operator=(MFCStringLike auto const* newValue) noexcept
    {
        value = std::string_view(newValue->GetString(), newValue->GetLength());
        return *this;
    }

    // Check if the value is NULL.
    [[nodiscard]] bool IsNull() const noexcept
    {
        return std::holds_alternative<SqlNullType>(value);
    }

    // Check if the value is of the specified type.
    template <typename T>
    [[nodiscard]] bool Is() const noexcept
    {
        return std::holds_alternative<T>(value);
    }

    // Retrieve the value as the specified type.
    template <typename T>
    [[nodiscard]] T& Get() noexcept
    {
        return std::get<T>(value);
    }

    // Retrieve the value as the specified type, or return the default value if the value is NULL.
    template <typename T>
    [[nodiscard]] T ValueOr(T&& defaultValue) const noexcept
    {
        if constexpr (std::is_integral_v<T>)
            return TryGetIntegral<T>().value_or(defaultValue);

        if (IsNull())
            return defaultValue;

        return std::get<T>(value);
    }

    // clang-format off
    [[nodiscard]] std::optional<bool> TryGetBool() const noexcept { return TryGetIntegral<bool>(); }
    [[nodiscard]] std::optional<short> TryGetShort() const noexcept { return TryGetIntegral<short>(); }
    [[nodiscard]] std::optional<unsigned short> TryGetUShort() const noexcept { return TryGetIntegral<unsigned short>(); }
    [[nodiscard]] std::optional<int> TryGetInt() const noexcept { return TryGetIntegral<int>(); }
    [[nodiscard]] std::optional<unsigned int> TryGetUInt() const noexcept { return TryGetIntegral<unsigned int>(); }
    [[nodiscard]] std::optional<long long> TryGetLongLong() const noexcept { return TryGetIntegral<long long>(); }
    [[nodiscard]] std::optional<unsigned long long> TryGetULongLong() const noexcept { return TryGetIntegral<unsigned long long>(); }
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

    [[nodiscard]] std::optional<SqlDate> TryGetDate() const
    {
        if (IsNull())
            return std::nullopt;

        if (auto const* date = std::get_if<SqlDate>(&value))
            return *date;

        throw std::bad_variant_access();
    }

    [[nodiscard]] std::optional<SqlTime> TryGetTime() const
    {
        if (IsNull())
            return std::nullopt;

        if (auto const* time = std::get_if<SqlTime>(&value))
            return *time;

        throw std::bad_variant_access();
    }

    [[nodiscard]] std::optional<SqlDateTime> TryGetDateTime() const
    {
        if (IsNull())
            return std::nullopt;

        if (auto const* dateTime = std::get_if<SqlDateTime>(&value))
            return *dateTime;

        throw std::bad_variant_access();
    }
};

template <>
struct SqlDataBinder<SqlVariant>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlVariant const& variantValue) noexcept
    {
        return std::visit(detail::overloaded { [&]<typename T>(T const& value) {
                              return SqlDataBinder<T>::InputParameter(stmt, column, value);
                          } },
                          variantValue.value);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlVariant* result, SQLLEN* indicator) noexcept
    {
        SQLLEN columnType {};
        SQLRETURN returnCode =
            SQLColAttributeA(stmt, static_cast<SQLSMALLINT>(column), SQL_DESC_TYPE, nullptr, 0, nullptr, &columnType);
        if (!SQL_SUCCEEDED(returnCode))
            return returnCode;

        auto& variant = result->value;

        switch (columnType)
        {
            case SQL_BIT:
                returnCode = SqlDataBinder<bool>::GetColumn(stmt, column, &variant.emplace<bool>(), indicator);
                break;
            case SQL_TINYINT:
                returnCode = SqlDataBinder<short>::GetColumn(stmt, column, &variant.emplace<short>(), indicator);
                break;
            case SQL_SMALLINT:
                returnCode = SqlDataBinder<unsigned short>::GetColumn(
                    stmt, column, &variant.emplace<unsigned short>(), indicator);
                break;
            case SQL_INTEGER:
                returnCode = SqlDataBinder<int>::GetColumn(stmt, column, &variant.emplace<int>(), indicator);
                break;
            case SQL_BIGINT:
                returnCode =
                    SqlDataBinder<long long>::GetColumn(stmt, column, &variant.emplace<long long>(), indicator);
                break;
            case SQL_REAL:
                returnCode = SqlDataBinder<float>::GetColumn(stmt, column, &variant.emplace<float>(), indicator);
                break;
            case SQL_FLOAT:
            case SQL_DOUBLE:
                returnCode = SqlDataBinder<double>::GetColumn(stmt, column, &variant.emplace<double>(), indicator);
                break;
            case SQL_CHAR:          // fixed-length string
            case SQL_VARCHAR:       // variable-length string
            case SQL_LONGVARCHAR:   // long string
            case SQL_WCHAR:         // fixed-length Unicode (UTF-16) string
            case SQL_WVARCHAR:      // variable-length Unicode (UTF-16) string
            case SQL_WLONGVARCHAR:  // long Unicode (UTF-16) string
            case SQL_BINARY:        // fixed-length binary
            case SQL_VARBINARY:     // variable-length binary
            case SQL_LONGVARBINARY: // long binary
                returnCode =
                    SqlDataBinder<std::string>::GetColumn(stmt, column, &variant.emplace<std::string>(), indicator);
                break;
            case SQL_DATE:
                SqlLogger::GetLogger().OnWarning(
                    std::format("SQL_DATE is from ODBC 2. SQL_TYPE_DATE should have been received instead."));
                [[fallthrough]];
            case SQL_TYPE_DATE:
                returnCode = SqlDataBinder<SqlDate>::GetColumn(stmt, column, &variant.emplace<SqlDate>(), indicator);
                break;
            case SQL_TIME:
                SqlLogger::GetLogger().OnWarning(
                    std::format("SQL_TIME is from ODBC 2. SQL_TYPE_TIME should have been received instead."));
                [[fallthrough]];
            case SQL_TYPE_TIME:
            case SQL_SS_TIME2:
                returnCode = SqlDataBinder<SqlTime>::GetColumn(stmt, column, &variant.emplace<SqlTime>(), indicator);
                break;
            case SQL_TYPE_TIMESTAMP:
                returnCode =
                    SqlDataBinder<SqlDateTime>::GetColumn(stmt, column, &variant.emplace<SqlDateTime>(), indicator);
                break;
            case SQL_TYPE_NULL:
            case SQL_DECIMAL:
            case SQL_NUMERIC:
            case SQL_GUID:
                // TODO: Get them implemented on demand
                [[fallthrough]];
            default:
                SqlLogger::GetLogger().OnError(SqlError::UNSUPPORTED_TYPE);
                returnCode = SQL_ERROR; // std::errc::invalid_argument;
        }
        if (indicator && *indicator == SQL_NULL_DATA)
            variant = SqlNullValue;
        return returnCode;
    }
};
