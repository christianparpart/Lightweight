// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"
#include "UnicodeConverter.hpp"

#include <cassert>
#include <memory>
#include <utility>

template <SqlBasicStringOperationsConcept StringType>
struct LIGHTWEIGHT_API SqlDataBinder<StringType>
{
    using ValueType = StringType;
    using StringTraits = SqlBasicStringOperations<ValueType>;

    static constexpr SqlColumnType ColumnType = StringTraits::ColumnType;

    static SQLRETURN InputParameter(SQLHSTMT stmt,
                                    SQLUSMALLINT column,
                                    ValueType const& value,
                                    SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                StringTraits::Size(&value),
                                0,
                                (SQLPOINTER) StringTraits::Data(&value),
                                0,
                                nullptr);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
    {
        cb.PlanPostProcessOutputColumn(
            [stmt, column, indicator, result]() { PostProcessOutputColumn(stmt, column, result, indicator); });
        return SQLBindCol(stmt,
                          column,
                          SQL_C_CHAR,
                          (SQLPOINTER) StringTraits::Data(result),
                          (SQLLEN) StringTraits::Size(result),
                          indicator);
    }

    static void PostProcessOutputColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator)
    {
        // Now resize the string to the actual length of the data
        // NB: If the indicator is greater than the buffer size, we have a truncation.
        if (*indicator == SQL_NO_TOTAL)
        {
            // We have a truncation and the server does not know how much data is left.
            StringTraits::Resize(result, StringTraits::Size(result) - 1);
        }
        else if (*indicator == SQL_NULL_DATA)
        {
            // We have a NULL value
            StringTraits::Resize(result, 0);
        }
        else if (*indicator <= static_cast<SQLLEN>(StringTraits::Size(result)))
        {
            StringTraits::Resize(result, static_cast<size_t>(*indicator));
        }
        else
        {
            // We have a truncation and the server knows how much data is left.
            // Extend the buffer and fetch the rest via SQLGetData.

            auto const totalCharsRequired = *indicator;
            StringTraits::Resize(result, totalCharsRequired + 1);
            auto const sqlResult =
                SQLGetData(stmt, column, SQL_C_CHAR, StringTraits::Data(result), totalCharsRequired + 1, indicator);
            (void) sqlResult;
            assert(SQL_SUCCEEDED(sqlResult));
            assert(*indicator == totalCharsRequired);
            StringTraits::Resize(result, totalCharsRequired);
        }
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt,
                               SQLUSMALLINT column,
                               ValueType* result,
                               SQLLEN* indicator,
                               SqlDataBinderCallback const& /*cb*/) noexcept
    {
        StringTraits::Reserve(result, 15);
        size_t writeIndex = 0;
        *indicator = 0;
        while (true)
        {
            auto* const bufferStart = StringTraits::Data(result) + writeIndex;
            size_t const bufferSize = StringTraits::Size(result) - writeIndex;
            SQLRETURN const rv = SQLGetData(stmt, column, SQL_C_CHAR, bufferStart, bufferSize, indicator);
            switch (rv)
            {
                case SQL_SUCCESS:
                case SQL_NO_DATA:
                    // last successive call
                    if (*indicator != SQL_NULL_DATA)
                    {
                        StringTraits::Resize(result, writeIndex + *indicator);
                        *indicator = StringTraits::Size(result);
                    }
                    return SQL_SUCCESS;
                case SQL_SUCCESS_WITH_INFO: {
                    // more data pending
                    if (*indicator == SQL_NO_TOTAL)
                    {
                        // We have a truncation and the server does not know how much data is left.
                        writeIndex += bufferSize - 1;
                        StringTraits::Resize(result, (2 * writeIndex) + 1);
                    }
                    else if (std::cmp_greater_equal(*indicator, bufferSize))
                    {
                        // We have a truncation and the server knows how much data is left.
                        writeIndex += bufferSize - 1;
                        StringTraits::Resize(result, writeIndex + *indicator);
                    }
                    else
                    {
                        // We have no truncation and the server knows how much data is left.
                        StringTraits::Resize(result, writeIndex + *indicator - 1);
                        return SQL_SUCCESS;
                    }
                    break;
                }
                default:
                    return rv;
            }
        }
    }

    static LIGHTWEIGHT_FORCE_INLINE std::string_view Inspect(ValueType const& value) noexcept
    {
        return { StringTraits::Data(&value), StringTraits::Size(&value) };
    }
};

template <SqlCommonWideStringBinderConcept StringType>
struct LIGHTWEIGHT_API SqlDataBinder<StringType>
{
    using ValueType = StringType;
    using StringTraits = SqlBasicStringOperations<ValueType>;

    static constexpr SqlColumnType ColumnType = StringTraits::ColumnType;

    static constexpr auto CType = SQL_C_WCHAR;
    static constexpr auto SqlType = SQL_WVARCHAR;

    static SQLRETURN InputParameter(SQLHSTMT stmt,
                                    SQLUSMALLINT column,
                                    ValueType const& value,
                                    SqlDataBinderCallback& cb) noexcept
    {
        switch (cb.ServerType())
        {
            case SqlServerType::POSTGRESQL: {
                // PostgreSQL only supports UTF-8 as Unicode encoding
                auto u8String = std::make_shared<std::u8string>(ToUtf8(detail::SqlViewHelper<ValueType>::View(value)));
                cb.PlanPostExecuteCallback([u8String = u8String]() {}); // Keep the string alive
                return SQLBindParameter(stmt,
                                        column,
                                        SQL_PARAM_INPUT,
                                        SQL_C_CHAR,
                                        SQL_VARCHAR,
                                        u8String->size(),
                                        0,
                                        (SQLPOINTER) u8String->data(),
                                        0,
                                        nullptr);
            }
            case SqlServerType::ORACLE:
            case SqlServerType::MYSQL:
            case SqlServerType::SQLITE: // We assume UTF-16 for SQLite
            case SqlServerType::MICROSOFT_SQL:
            case SqlServerType::UNKNOWN: {
                using CharType = StringTraits::CharType;
                auto const* data = StringTraits::Data(&value);
                auto const sizeInBytes = StringTraits::Size(&value) * sizeof(CharType);
                return SQLBindParameter(
                    stmt, column, SQL_PARAM_INPUT, CType, SqlType, sizeInBytes, 0, (SQLPOINTER) data, 0, nullptr);
            }
        }
        std::unreachable();
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback& cb) noexcept
    {
        // Ensure we're having sufficient space to store the worst-case scenario of bytes in this column
        SQLULEN columnSize {};
        auto const describeResult = SQLDescribeCol(stmt,
                                                   column,
                                                   nullptr /*colName*/,
                                                   0 /*sizeof(colName)*/,
                                                   nullptr /*&colNameLen*/,
                                                   nullptr /*&dataType*/,
                                                   &columnSize,
                                                   nullptr /*&decimalDigits*/,
                                                   nullptr /*&nullable*/);
        if (!SQL_SUCCEEDED(describeResult))
            return describeResult;

        StringTraits::Reserve(result,
                              columnSize); // Must be called now, because otherwise std::string won't do anything

        cb.PlanPostProcessOutputColumn([indicator, result]() {
            // Now resize the string to the actual length of the data
            // NB: If the indicator is greater than the buffer size, we have a truncation.
            if (*indicator != SQL_NULL_DATA)
            {
                auto const bufferSize = StringTraits::Size(result);
                auto const len = std::cmp_greater_equal(*indicator, bufferSize) || *indicator == SQL_NO_TOTAL
                                     ? bufferSize - 1
                                     : *indicator;
                StringTraits::Resize(result, len / sizeof(decltype(StringTraits::Data(result)[0])));
            }
            else
                StringTraits::Resize(result, 0);
        });
        return SQLBindCol(stmt,
                          column,
                          CType,
                          (SQLPOINTER) StringTraits::Data(result),
                          (SQLLEN) StringTraits::Size(result),
                          indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt,
                               SQLUSMALLINT column,
                               ValueType* result,
                               SQLLEN* indicator,
                               SqlDataBinderCallback const& /*cb*/) noexcept
    {
        using CharType = decltype(StringTraits::Data(result)[0]);

        StringTraits::Reserve(result, 60);
        *indicator = 0;

        // Resize the string to the size of the data
        // Get the data (take care of SQL_NULL_DATA and SQL_NO_TOTAL)
        auto sqlResult = SQLGetData(stmt,
                                    column,
                                    CType,
                                    (SQLPOINTER) StringTraits::Data(result),
                                    StringTraits::Size(result) * sizeof(CharType),
                                    indicator);

        if (sqlResult == SQL_SUCCESS || sqlResult == SQL_NO_DATA)
        {
            // Data has been read successfully on first call to SQLGetData, or there is no data to read.
            if (*indicator == SQL_NULL_DATA)
                StringTraits::Resize(result, 0);
            else
                StringTraits::Resize(result, *indicator / sizeof(CharType));
            return sqlResult;
        }

        if (sqlResult == SQL_SUCCESS_WITH_INFO && *indicator > static_cast<SQLLEN>(StringTraits::Size(result)))
        {
            // We have a truncation and the server knows how much data is left.
            auto const totalCharCount = *indicator / sizeof(CharType);
            auto const charsWritten = StringTraits::Size(result) - 1;
            StringTraits::Resize(result, totalCharCount + 1);
            auto* bufferCont = StringTraits::Data(result) + charsWritten;
            auto const bufferCharsAvailable = (totalCharCount + 1) - charsWritten;
            sqlResult = SQLGetData(stmt, column, CType, bufferCont, bufferCharsAvailable * sizeof(CharType), indicator);
            if (SQL_SUCCEEDED(sqlResult))
                StringTraits::Resize(result, charsWritten + *indicator / sizeof(CharType));
            return sqlResult;
        }

        size_t writeIndex = 0;
        while (sqlResult == SQL_SUCCESS_WITH_INFO && *indicator == SQL_NO_TOTAL)
        {
            // We have a truncation and the server does not know how much data is left.
            writeIndex += StringTraits::Size(result) - 1;
            StringTraits::Resize(result, StringTraits::Size(result) * 2);
            auto* const bufferStart = StringTraits::Data(result) + writeIndex;
            size_t const bufferCharsAvailable = StringTraits::Size(result) - writeIndex;
            sqlResult = SQLGetData(stmt, column, CType, bufferStart, bufferCharsAvailable, indicator);
        }
        return sqlResult;
    }

    static LIGHTWEIGHT_FORCE_INLINE std::string Inspect(ValueType const& value) noexcept
    {
        auto u8String = ToUtf8(detail::SqlViewHelper<ValueType>::View(value));
        return std::string((char const*) u8String.data(), u8String.size());
    }
};
