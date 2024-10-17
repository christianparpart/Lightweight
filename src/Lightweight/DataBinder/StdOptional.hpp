// SPDX-License-Identifier: MIT
#pragma once

#include "Core.hpp"

#include <concepts>
#include <optional>

template <typename T>
    requires(std::is_same_v<SqlDataBinder<T>, SqlDataBinder<T>>)
struct SqlDataBinder<std::optional<T>>
{
    using OptionalValue = std::optional<T>;
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLUSMALLINT column, OptionalValue const& value) noexcept
    {
        if (value.has_value())
            return SqlDataBinder<T>::InputParameter(stmt, column, *value);
        else
            return SqlDataBinder<SqlNullType>::InputParameter(stmt, column, SqlNullValue);
    }

    static SQLRETURN OutputColumn(SQLHSTMT stmt,
                                  SQLUSMALLINT column,
                                  OptionalValue* result,
                                  SQLLEN* indicator,
                                  SqlDataBinderCallback& cb) noexcept
    {
        if (!result)
            return SQL_ERROR;

        auto const sqlReturn = SqlDataBinder<T>::OutputColumn(stmt, column, &result->emplace(), indicator, cb);
        cb.PlanPostProcessOutputColumn([result, indicator]() {
            if (indicator && *indicator == SQL_NULL_DATA)
                *result = std::nullopt;
        });
        return sqlReturn;
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLUSMALLINT column, OptionalValue* result, SQLLEN* indicator) noexcept
    {
        auto const sqlReturn = SqlDataBinder<T>::GetColumn(stmt, column, &result->emplace(), indicator);
        if (indicator && *indicator == SQL_NULL_DATA)
            *result = std::nullopt;
        return sqlReturn;
    }
};
