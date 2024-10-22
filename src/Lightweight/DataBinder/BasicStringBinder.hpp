// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

#include <utility>

template <SqlCommonStringBinderConcept StringType>
struct SqlDataBinder<StringType>
{
    using ValueType = StringType;
    using StringTraits = SqlCommonStringBinder<ValueType>;

    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, ValueType const& value) noexcept
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
                StringTraits::Resize(result, len);
            }
            else
                StringTraits::Resize(result, 0);
        });
        return SQLBindCol(stmt,
                          column,
                          SQL_C_CHAR,
                          (SQLPOINTER) StringTraits::Data(result),
                          (SQLLEN) StringTraits::Size(result),
                          indicator);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator) noexcept
    {
        StringTraits::Reserve(result, 15);
        size_t writeIndex = 0;
        *indicator = 0;
        while (true)
        {
            char* const bufferStart = StringTraits::Data(result) + writeIndex;
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
};
