// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../DataBinder/Core.hpp"
#include "../SqlStatement.hpp"

#include <reflection-cpp/reflection.hpp>

enum class FieldValueRequirement : uint8_t
{
    NULLABLE,
    NOT_NULL,
};

constexpr inline FieldValueRequirement SqlNullable = FieldValueRequirement::NULLABLE;
constexpr inline FieldValueRequirement SqlNotNullable = FieldValueRequirement::NULLABLE;

/// @brief Tells the data mapper that this field is a primary key with given semantics, or not a primary key.
enum class PrimaryKey : uint8_t
{
    /// The field is not a primary key.
    No,

    /// @brief The field is a primary key.
    ///
    /// If the field is an auto-incrementable key and not manually set, it is automatically set to the
    /// next available value on the client side, using a SELECT MAX() query.
    /// This is happening transparently to the user.
    ///
    /// If the field is a GUID, it is automatically set to a new GUID value, if not manually set.
    ///
    /// @note If the field is neither auto-incrementable nor a GUID, it must be manually set.
    AutoAssign,

    /// The field is an integer primary key, and it is auto-incremented by the database.
    ServerSideAutoIncrement,
};

namespace detail
{

// clang-format off

template <typename T>
struct IsStdOptionalType: std::false_type {};

template <typename T>
struct IsStdOptionalType<std::optional<T>>: std::true_type {};

template <typename T>
constexpr bool IsStdOptional = IsStdOptionalType<T>::value;

template <typename T>
concept FieldElementType = SqlInputParameterBinder<T> && SqlOutputColumnBinder<T>;

// clang-format on

} // namespace detail

/// @brief Represents a single column in a table.
///
/// The column name, index, and type are known at compile time.
/// If either name or index are not known at compile time, leave them at their default values,
/// but at least one of them msut be known.
///
/// It is imperative that this data structure is an aggregate type, such that it works with C++20 reflection.
///
/// @see DataMapper
/// @ingroup DataMapper
template <detail::FieldElementType T, PrimaryKey IsPrimaryKeyValue = PrimaryKey::No>
struct Field
{
    using ValueType = T;

    // clang-format off
    constexpr Field() noexcept = default;
    constexpr Field(Field const&) noexcept = default;
    constexpr Field& operator=(Field const&) noexcept = default;
    constexpr Field(Field&&) noexcept = default;
    constexpr Field& operator=(Field&&) noexcept = default;
    constexpr ~Field() noexcept = default;
    // clang-format on

    template <typename... S>
        requires std::constructible_from<T, S...>
    constexpr Field(S&&... value) noexcept;

    template <typename S>
        requires std::constructible_from<T, S>
    constexpr Field& operator=(S&& value) noexcept;

    static constexpr auto IsOptional = detail::IsStdOptional<T>;
    static constexpr auto IsMandatory = !IsOptional;
    static constexpr auto IsPrimaryKey = IsPrimaryKeyValue != PrimaryKey::No;
    static constexpr auto IsAutoAssignPrimaryKey = IsPrimaryKeyValue == PrimaryKey::AutoAssign;
    static constexpr auto IsAutoIncrementPrimaryKey = IsPrimaryKeyValue == PrimaryKey::ServerSideAutoIncrement;

    constexpr std::weak_ordering operator<=>(Field const& other) const noexcept;

    bool operator==(Field const& value) const noexcept = default;
    bool operator!=(Field const& value) const noexcept = default;

    bool operator==(T const& value) const noexcept;
    bool operator!=(T const& value) const noexcept;

    /// Returns a string representation of the value, suitable for use in debugging and logging.
    [[nodiscard]] std::string InspectValue() const;

    /// Sets the modified state of the field.
    constexpr void SetModified(bool value) noexcept;

    /// Checks if the field has been modified.
    [[nodiscard]] constexpr bool IsModified() const noexcept;

    /// Returns the value of the field.
    [[nodiscard]] constexpr T const& Value() const noexcept;

    /// Returns a mutable reference to the value of the field.
    ///
    /// @note If the field value is changed through this method, it will not be automatically marked as modified.
    [[nodiscard]] constexpr T& MutableValue() noexcept;

  private:
    ValueType _value {};
    bool _modified { false };
};

// clang-format off
namespace detail
{

template <typename T>
struct IsAutoIncrementPrimaryKeyField: std::false_type {};

template <typename T>
struct IsAutoIncrementPrimaryKeyField<Field<T, PrimaryKey::ServerSideAutoIncrement>>: std::true_type {};

template <typename T>
struct IsFieldType: std::false_type {};

template <typename T, PrimaryKey IsPrimaryKey>
struct IsFieldType<Field<T, IsPrimaryKey>>: std::true_type {};

} // namespace detail
// clang-format on

// Requires that T satisfies to be a field with storage and is considered a primary key.
template <typename T>
constexpr bool IsAutoIncrementPrimaryKey = detail::IsAutoIncrementPrimaryKeyField<T>::value;

template <typename T>
constexpr bool IsField = detail::IsFieldType<std::remove_cvref_t<T>>::value;

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
template <typename... S>
    requires std::constructible_from<T, S...>
constexpr LIGHTWEIGHT_FORCE_INLINE Field<T, IsPrimaryKey>::Field(S&&... value) noexcept:
    _value(std::forward<S>(value)...)
{
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
template <typename S>
    requires std::constructible_from<T, S>
constexpr LIGHTWEIGHT_FORCE_INLINE Field<T, IsPrimaryKey>& Field<T, IsPrimaryKey>::operator=(S&& value) noexcept
{
    _value = std::forward<S>(value);
    SetModified(true);
    return *this;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
constexpr std::weak_ordering LIGHTWEIGHT_FORCE_INLINE
Field<T, IsPrimaryKey>::operator<=>(Field const& other) const noexcept
{
    return _value <=> other._value;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
inline LIGHTWEIGHT_FORCE_INLINE bool Field<T, IsPrimaryKey>::operator==(T const& value) const noexcept
{
    return _value == value;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
inline LIGHTWEIGHT_FORCE_INLINE bool Field<T, IsPrimaryKey>::operator!=(T const& value) const noexcept
{
    return _value != value;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
inline LIGHTWEIGHT_FORCE_INLINE std::string Field<T, IsPrimaryKey>::InspectValue() const
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::stringstream result;
        result << std::quoted(_value, '\'');
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

// ------------------------------------------------------------------------------------------------

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
constexpr LIGHTWEIGHT_FORCE_INLINE void Field<T, IsPrimaryKey>::SetModified(bool value) noexcept
{
    _modified = value;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
constexpr LIGHTWEIGHT_FORCE_INLINE bool Field<T, IsPrimaryKey>::IsModified() const noexcept
{
    return _modified;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
constexpr LIGHTWEIGHT_FORCE_INLINE T const& Field<T, IsPrimaryKey>::Value() const noexcept
{
    return _value;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
constexpr LIGHTWEIGHT_FORCE_INLINE T& Field<T, IsPrimaryKey>::MutableValue() noexcept
{
    return _value;
}

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
struct SqlDataBinder<Field<T, IsPrimaryKey>>
{
    using ValueType = Field<T, IsPrimaryKey>;

    static constexpr auto ColumnType = SqlDataBinder<T>::ColumnType;

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN InputParameter(SQLHSTMT stmt,
                                                             SQLUSMALLINT column,
                                                             ValueType const& value,
                                                             SqlDataBinderCallback& cb)
    {
        return SqlDataBinder<T>::InputParameter(stmt, column, value.Value(), cb);
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN
    OutputColumn(SQLHSTMT stmt, SQLUSMALLINT column, ValueType* result, SQLLEN* indicator, SqlDataBinderCallback& cb)
    {
        auto const sqlReturn = SqlDataBinder<T>::OutputColumn(stmt, column, &result->MutableValue(), indicator, cb);
        cb.PlanPostProcessOutputColumn([result]() { result->SetModified(true); });
        return sqlReturn;
    }

    static LIGHTWEIGHT_FORCE_INLINE SQLRETURN GetColumn(SQLHSTMT stmt,
                                                        SQLUSMALLINT column,
                                                        ValueType* result,
                                                        SQLLEN* indicator,
                                                        SqlDataBinderCallback const& cb) noexcept
    {
        auto const sqlReturn = SqlDataBinder<T>::GetColumn(stmt, column, &result->emplace(), indicator, cb);
        if (SQL_SUCCEEDED(sqlReturn))
            result->SetModified(true);
        return sqlReturn;
    }

    static LIGHTWEIGHT_FORCE_INLINE std::string Inspect(ValueType const& value) noexcept
    {
        return value.InspectValue();
    }
};

template <detail::FieldElementType T, PrimaryKey IsPrimaryKey>
struct std::formatter<Field<T, IsPrimaryKey>>: std::formatter<T>
{
    template <typename FormatContext>
    auto format(Field<T, IsPrimaryKey> const& field, FormatContext& ctx)
    {
        return formatter<T>::format(field.InspectValue(), ctx);
    }
};
