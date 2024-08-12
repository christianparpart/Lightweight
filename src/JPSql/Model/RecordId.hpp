#pragma once

#include "../../JpSql/SqlDataBinder.hpp"

#include <format>

namespace Model
{

// Represents a unique identifier of a specific record in a table.
struct RecordId
{
    size_t value;

    constexpr size_t operator*() const noexcept
    {
        return value;
    }

    constexpr std::strong_ordering operator<=>(RecordId const& other) const noexcept = default;
};

template <>
struct SqlDataBinder<RecordId>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLSMALLINT column, RecordId const& value)
    {
        return SqlDataBinder<decltype(value.value)>::InputParameter(stmt, column, value.value);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLSMALLINT column, RecordId* result, SQLLEN* indicator, SqlDataBinderCallback& cb)
    {
        return SqlDataBinder<decltype(result->value)>::OutputColumn(stmt, column, &result->value, indicator, cb);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLSMALLINT column, RecordId* result, SQLLEN* indicator)
    {
        return SqlDataBinder<decltype(result->value)>::GetColumn(stmt, column, &result->value, indicator);
    }
};

} // namespace Model

template <>
struct std::formatter<Model::RecordId>: std::formatter<size_t>
{
    template <typename FormatContext>
    auto format(Model::RecordId id, format_context& ctx) const -> format_context::iterator
    {
        return formatter<size_t>::format(id.value, ctx);
    }
};
