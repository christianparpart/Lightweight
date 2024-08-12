#pragma once

#include "../../JpSql/SqlDataBinder.hpp"

namespace Model
{

struct SqlModelId
{
    size_t value;

    constexpr size_t operator*() const noexcept
    {
        return value;
    }
    constexpr std::strong_ordering operator<=>(SqlModelId const& other) const noexcept = default;
};

template <>
struct SqlDataBinder<SqlModelId>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLSMALLINT column, SqlModelId const& value)
    {
        return SqlDataBinder<decltype(value.value)>::InputParameter(stmt, column, value.value);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLSMALLINT column, SqlModelId* result, SQLLEN* indicator, SqlDataBinderCallback& cb)
    {
        return SqlDataBinder<decltype(result->value)>::OutputColumn(stmt, column, &result->value, indicator, cb);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLSMALLINT column, SqlModelId* result, SQLLEN* indicator)
    {
        return SqlDataBinder<decltype(result->value)>::GetColumn(stmt, column, &result->value, indicator);
    }
}

} // namespace Model
