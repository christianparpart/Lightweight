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
    static SQLRETURN InputParameter(SQLHSTMT stmt,
                                    SQLUSMALLINT column,
                                    SqlNullType const& value,
                                    SqlDataBinderCallback& /*cb*/) noexcept
    {
        return SQLBindParameter(stmt,
                                column,
                                SQL_PARAM_INPUT,
                                SQL_C_CHAR,
                                SQL_VARCHAR,
                                0,
                                0,
                                nullptr,
                                0,
                                &const_cast<SqlNullType&>(value).sqlValue);
    }
};

template <>
struct SqlDataTraits<SqlNullType>
{
    static constexpr unsigned Size = 0;
    static constexpr SqlColumnType Type = SqlColumnType::UNKNOWN;
};
