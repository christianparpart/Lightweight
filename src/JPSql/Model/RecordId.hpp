// SPDX-License-Identifier: MIT
#pragma once

#include "../../JPSql/SqlDataBinder.hpp"

#include <format>

namespace Model
{

// Represents a unique identifier of a specific record in a table.
struct RecordId
{
    using InnerType = size_t;
    InnerType value;

    constexpr InnerType operator*() const noexcept
    {
        return value;
    }

    constexpr std::weak_ordering operator<=>(RecordId const& other) const noexcept = default;

    constexpr bool operator==(RecordId other) const noexcept
    {
        return value == other.value;
    }

    constexpr bool operator==(InnerType other) const noexcept
    {
        return value == other;
    }
};

} // namespace Model

template <typename>
struct WhereConditionLiteralType;

template <>
struct WhereConditionLiteralType<Model::RecordId>
{
    constexpr static bool needsQuotes = false;
};

template <>
struct SqlDataBinder<Model::RecordId>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLSMALLINT column, Model::RecordId const& value)
    {
        return SqlDataBinder<decltype(value.value)>::InputParameter(stmt, column, value.value);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLSMALLINT column, Model::RecordId* result, SQLLEN* indicator, SqlDataBinderCallback& cb)
    {
        return SqlDataBinder<decltype(result->value)>::OutputColumn(stmt, column, &result->value, indicator, cb);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLSMALLINT column, Model::RecordId* result, SQLLEN* indicator)
    {
        return SqlDataBinder<decltype(result->value)>::GetColumn(stmt, column, &result->value, indicator);
    }
};

template <>
struct std::formatter<Model::RecordId>: std::formatter<Model::RecordId::InnerType>
{
    auto format(Model::RecordId id, format_context& ctx) const -> format_context::iterator
    {
        return formatter<Model::RecordId::InnerType>::format(id.value, ctx);
    }
};
