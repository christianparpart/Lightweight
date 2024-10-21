// SPDX-License-Identifier: MIT
#pragma once

#include "../SqlLogger.hpp"
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

// Helper struct to store a timestamp that should be automatically converted to/from a SQL_TIMESTAMP_STRUCT.
// Helper struct to generically store and load a variant of different SQL types.
using SqlVariant = std::variant<SqlNullType,
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

template <>
struct SqlDataBinder<SqlVariant>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, SqlVariant const& variantValue) noexcept
    {
        return std::visit(detail::overloaded { [&]<typename T>(T const& value) {
                              return SqlDataBinder<T>::InputParameter(stmt, column, value);
                          } },
                          variantValue);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, SqlVariant* result, SQLLEN* indicator) noexcept
    {
        SQLLEN columnType {};
        SQLRETURN returnCode =
            SQLColAttributeA(stmt, static_cast<SQLSMALLINT>(column), SQL_DESC_TYPE, nullptr, 0, nullptr, &columnType);
        if (!SQL_SUCCEEDED(returnCode))
            return returnCode;

        switch (columnType)
        {
            case SQL_BIT:
                result->emplace<bool>();
                returnCode = SqlDataBinder<bool>::GetColumn(stmt, column, &std::get<bool>(*result), indicator);
                break;
            case SQL_TINYINT:
                result->emplace<short>();
                returnCode = SqlDataBinder<short>::GetColumn(stmt, column, &std::get<short>(*result), indicator);
                break;
            case SQL_SMALLINT:
                result->emplace<unsigned short>();
                returnCode = SqlDataBinder<unsigned short>::GetColumn(
                    stmt, column, &std::get<unsigned short>(*result), indicator);
                break;
            case SQL_INTEGER:
                result->emplace<int>();
                returnCode = SqlDataBinder<int>::GetColumn(stmt, column, &std::get<int>(*result), indicator);
                break;
            case SQL_BIGINT:
                result->emplace<long long>();
                returnCode =
                    SqlDataBinder<long long>::GetColumn(stmt, column, &std::get<long long>(*result), indicator);
                break;
            case SQL_REAL:
                result->emplace<float>();
                returnCode = SqlDataBinder<float>::GetColumn(stmt, column, &std::get<float>(*result), indicator);
                break;
            case SQL_FLOAT:
            case SQL_DOUBLE:
                result->emplace<double>();
                returnCode = SqlDataBinder<double>::GetColumn(stmt, column, &std::get<double>(*result), indicator);
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
                result->emplace<std::string>();
                returnCode =
                    SqlDataBinder<std::string>::GetColumn(stmt, column, &std::get<std::string>(*result), indicator);
                break;
            case SQL_DATE:
                SqlLogger::GetLogger().OnWarning(
                    std::format("SQL_DATE is from ODBC 2. SQL_TYPE_DATE should have been received instead."));
                [[fallthrough]];
            case SQL_TYPE_DATE:
                result->emplace<SqlDate>();
                returnCode = SqlDataBinder<SqlDate>::GetColumn(stmt, column, &std::get<SqlDate>(*result), indicator);
                break;
            case SQL_TIME:
                SqlLogger::GetLogger().OnWarning(
                    std::format("SQL_TIME is from ODBC 2. SQL_TYPE_TIME should have been received instead."));
                [[fallthrough]];
            case SQL_TYPE_TIME:
            case SQL_SS_TIME2:
                result->emplace<SqlTime>();
                returnCode = SqlDataBinder<SqlTime>::GetColumn(stmt, column, &std::get<SqlTime>(*result), indicator);
                break;
            case SQL_TYPE_TIMESTAMP:
                result->emplace<SqlDateTime>();
                returnCode =
                    SqlDataBinder<SqlDateTime>::GetColumn(stmt, column, &std::get<SqlDateTime>(*result), indicator);
                break;
            case SQL_TYPE_NULL:
            case SQL_DECIMAL:
            case SQL_NUMERIC:
            case SQL_GUID:
                // TODO: Get them implemented on demand
                [[fallthrough]];
            default:
                std::println("Unsupported column type: {}", columnType);
                SqlLogger::GetLogger().OnError(SqlError::UNSUPPORTED_TYPE);
                returnCode = SQL_ERROR; // std::errc::invalid_argument;
        }
        if (indicator && *indicator == SQL_NULL_DATA)
            *result = SqlNullValue;
        return returnCode;
    }
};
