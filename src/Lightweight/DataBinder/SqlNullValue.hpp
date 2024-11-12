// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Core.hpp"

// Helper binder type to indicate NULL values in SQL queries.
struct SqlNullType
{
    SQLLEN sqlValue = SQL_NULL_DATA;
};

// Used to indicate a NULL value in a SQL query.
constexpr auto SqlNullValue = SqlNullType {};

template <>
struct SqlDataBinder<SqlNullType>
{
    static constexpr auto ColumnType = SqlColumnType::UNKNOWN;

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt,
                                                             SQLUSMALLINT column,
                                                             SqlNullType const& value,
                                                             SqlDataBinderCallback& /*cb*/) noexcept
    {
        // This is generally ignored for NULL values, but MS SQL Server requires a non-zero value
        // when the underlying type is e.g. an INT.
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                10,
                                0,
                                nullptr,
                                0,
                                &const_cast<SqlNullType&>(value).sqlValue);
    }
};
