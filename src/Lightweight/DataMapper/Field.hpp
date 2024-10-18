// SPDX-License-Identifier: MIT
#pragma once

#include <Lightweight/SqlStatement.hpp>

#include <reflection-cpp/reflection.hpp>

namespace DataMapper
{

struct SqlColumnNameView
{
    std::string_view name;
};

enum class FieldValueRequirement : uint8_t
{
    NULLABLE,
    NOT_NULL,
};

constexpr inline FieldValueRequirement SqlNullable = FieldValueRequirement::NULLABLE;
constexpr inline FieldValueRequirement SqlNotNullable = FieldValueRequirement::NULLABLE;

// Represents a single column in a table.
//
// The column name, index, and type are known at compile time.
// If either name or index are not known at compile time, leave them at their default values,
// but at least one of them msut be known.
//
// It is imperative that this data structure is an aggregate type, such that it works with C++20 reflection.
template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement = SqlNotNullable>
struct Field
{
    using value_type = T;

    value_type _value {};
    bool _modified { false };

    static constexpr inline auto TableColumnIndex = TheTableColumnIndex;
    static constexpr inline auto ColumnName = TheColumnName;
    static constexpr inline auto Requirement = TheRequirement;

    static constexpr inline auto Type = SqlDataTraits<T>::Type;
    static constexpr inline auto Size = SqlDataTraits<T>::Size;

    Field& operator=(T const& value);
    Field& operator=(T&& value) noexcept;

    bool operator==(Field const& value) const noexcept = default;
    bool operator!=(Field const& value) const noexcept = default;

    bool operator==(T const& value) const noexcept;
    bool operator!=(T const& value) const noexcept;

    // Returns a string representation of the value, suitable for use in debugging and logging.
    [[nodiscard]] std::string InspectValue() const;

    void BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const;
    void BindOutputColumn(SqlStatement& stmt);
    void BindOutputColumn(SQLSMALLINT outputIndex, SqlStatement& stmt);

    constexpr void SetModified(bool value) noexcept;
    [[nodiscard]] constexpr bool IsModified() const noexcept;
    [[nodiscard]] constexpr SQLSMALLINT Index() const noexcept;
    [[nodiscard]] constexpr SqlColumnNameView Name() const noexcept;
    [[nodiscard]] constexpr T const& Value() const noexcept;
    [[nodiscard]] constexpr bool IsNullable() const noexcept;
    [[nodiscard]] constexpr bool IsRequired() const noexcept;
};

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>& Field<T,
                                                                    TheTableColumnIndex,
                                                                    TheColumnName,
                                                                    TheRequirement>::operator=(T const& value)
{
    _value = value;
    SetModified(true);
    return *this;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>& Field<T,
                                                                    TheTableColumnIndex,
                                                                    TheColumnName,
                                                                    TheRequirement>::operator=(T&& value) noexcept
{
    _value = std::move(value);
    SetModified(true);
    return *this;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline bool Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::operator==(T const& value) const noexcept
{
    return _value == value;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline bool Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::operator!=(T const& value) const noexcept
{
    return _value != value;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
std::string Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::InspectValue() const
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::stringstream result;
        result << std::quoted(_value, '\'');
        return result.str();
    }
    else if constexpr (std::is_same_v<T, SqlTrimmedString>)
    {
        std::stringstream result;
        result << std::quoted(_value.value, '\'');
        return result.str();
    }
    else if constexpr (std::is_same_v<T, SqlText>)
    {
        std::stringstream result;
        result << std::quoted(_value.value, '\'');
        return result.str();
    }
    else if constexpr (std::is_same_v<T, SqlDate>)
        return std::format("\'{}\'", _value.value);
    else if constexpr (std::is_same_v<T, SqlTime>)
        return std::format("\'{}\'", _value.value);
    else if constexpr (std::is_same_v<T, SqlDateTime>)
        return std::format("\'{}\'", _value.value());
    else
        return std::format("{}", _value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
void Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindInputParameter(SQLSMALLINT parameterIndex,
                                                                                      SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, _value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
void Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindOutputColumn(SqlStatement& stmt)
{
    stmt.BindOutputColumn(TheTableColumnIndex, &_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
void Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindOutputColumn(SQLSMALLINT outputIndex,
                                                                                    SqlStatement& stmt)
{
    stmt.BindOutputColumn(outputIndex, &_value);
}

// ------------------------------------------------------------------------------------------------

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline constexpr void Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::SetModified(bool value) noexcept
{
    _modified = value;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline constexpr bool Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::IsModified() const noexcept
{
    return _modified;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline constexpr SQLSMALLINT Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::Index() const noexcept
{
    return TableColumnIndex;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline constexpr SqlColumnNameView Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::Name() const noexcept
{
    return ColumnName;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline constexpr T const& Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::Value() const noexcept
{
    return _value;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline constexpr bool Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::IsNullable() const noexcept
{
    return Requirement == FieldValueRequirement::NULLABLE;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
inline constexpr bool Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::IsRequired() const noexcept
{
    return Requirement == FieldValueRequirement::NOT_NULL;
}

} // namespace DataMapper

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          Reflection::StringLiteral TheColumnName,
          DataMapper::FieldValueRequirement TheRequirement>
struct std::formatter<DataMapper::Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>>: std::formatter<T>
{
    template <typename FormatContext>
    auto format(DataMapper::Field<T, TheTableColumnIndex, TheColumnName, TheRequirement> const& field, FormatContext& ctx)
    {
        return formatter<T>::format(field.InspectValue(), ctx);
    }
};
