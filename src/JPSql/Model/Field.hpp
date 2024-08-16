#pragma once

#include "../SqlStatement.hpp"
#include "AbstractField.hpp"
#include "AbstractRecord.hpp"
#include "ColumnType.hpp"
#include "RecordId.hpp"
#include "StringLiteral.hpp"

#include <string_view>

namespace Model
{

template <typename T>
struct Record;

// Represents a single column in a table.
//
// The column name, index, and type are known at compile time.
// If either name or index are not known at compile time, leave them at their default values,
// but at least one of them msut be known.
template <typename T,
          SQLSMALLINT TheTableColumnIndex = 0,
          StringLiteral TheColumnName = "",
          FieldValueRequirement TheRequirement = FieldValueRequirement::NOT_NULL>
class Field final: public AbstractField
{
  public:
    explicit Field(AbstractRecord& record):
        AbstractField {
            record, TheTableColumnIndex, TheColumnName.value, ColumnTypeOf<T>, TheRequirement,
        }
    {
        record.RegisterField(*this);
    }

    Field(Field const& other):
        AbstractField {
            const_cast<Field&>(other).GetRecord(),
            TheTableColumnIndex,
            TheColumnName.value,
            ColumnTypeOf<T>,
            TheRequirement,
        },
        m_value { other.m_value }
    {
        GetRecord().RegisterField(*this);
    }

    explicit Field(AbstractRecord& record, Field&& field):
        AbstractField {
            record, TheTableColumnIndex, TheColumnName.value, ColumnTypeOf<T>, TheRequirement,
        },
        m_value { std::move(field.m_value) }
    {
        field.GetRecord().UnregisterField(*this);
        record.RegisterField(*this);
    }

    Field() = delete;
    Field(Field&& other) = delete;
    Field& operator=(Field&& other) = delete;
    Field& operator=(Field const& other) = delete;
    ~Field() = default;

    // clang-format off

    template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
    auto operator<=>(Field<U, I, N, R> const& other) const noexcept { return m_value <=> other.m_value; }

    // We also define the equality and inequality operators explicitly, because <=> from above does not seem to work in MSVC VS 2022.
    template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
    auto operator==(Field<U, I, N, R> const& other) const noexcept { return m_value == other.m_value; }

    template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
    auto operator!=(Field<U, I, N, R> const& other) const noexcept { return m_value != other.m_value; }

    T const& Value() const noexcept { return m_value; }
    void SetData(T&& value) { SetModified(true); m_value = std::move(value); }
    void SetNull() { SetModified(true); m_value = T {}; }

    Field& operator=(T&& value) noexcept;

    T& operator*() noexcept { return m_value; }
    T const& operator*() const noexcept { return m_value; }

    // clang-format on

    std::string InspectValue() const override;
    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;
    SqlResult<void> BindOutputColumn(SQLSMALLINT index, SqlStatement& stmt) override;

  private:
    T m_value {};
};

// Represents a column in a table that is a foreign key to another table.
template <typename ModelType,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement = FieldValueRequirement::NOT_NULL>
class BelongsTo final: public AbstractField
{
  public:
    explicit BelongsTo(AbstractRecord& record):
        AbstractField {
            record, TheColumnIndex, TheForeignKeyName.value, ColumnTypeOf<RecordId>, TheRequirement,
        }
    {
        record.RegisterField(*this);
    }

    BelongsTo(BelongsTo const& other):
        AbstractField {
            const_cast<BelongsTo&>(other).GetRecord(),
            TheColumnIndex,
            TheForeignKeyName.value,
            ColumnTypeOf<RecordId>,
            TheRequirement,
        },
        m_value { other.m_value }
    {
        GetRecord().RegisterField(*this);
    }

    ~BelongsTo()
    {
        // TODO?: GetRecord().UnregisterField(*this);
    }

    explicit BelongsTo(AbstractRecord& record, BelongsTo&& other):
        AbstractField { std::move(other) },
        m_value { other.m_value }
    {
        record.RegisterField(*this);
    }

    BelongsTo& operator=(RecordId modelId) noexcept;
    BelongsTo& operator=(Record<ModelType> const& model) noexcept;

    ModelType* operator->() noexcept; // TODO
    ModelType& operator*() noexcept;  // TODO

    constexpr static inline SQLSMALLINT ColumnIndex { TheColumnIndex };
    constexpr static inline std::string_view ColumnName { TheForeignKeyName.value };

    std::string InspectValue() const override;
    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;
    SqlResult<void> BindOutputColumn(SQLSMALLINT index, SqlStatement& stmt) override;

    auto operator<=>(BelongsTo const& other) const noexcept
    {
        return m_value <=> other.m_value;
    }

    template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
    bool operator==(BelongsTo<U, I, N, R> const& other) const noexcept
    {
        return m_value == other.m_value;
    }

    template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
    bool operator!=(BelongsTo<U, I, N, R> const& other) const noexcept
    {
        return m_value == other.m_value;
    }

  private:
    RecordId m_value {};
};

#pragma region Field<> implementation

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>& Field<T,
                                                                    TheTableColumnIndex,
                                                                    TheColumnName,
                                                                    TheRequirement>::operator=(T&& value) noexcept
{
    SetModified(true);
    m_value = std::move(value);
    return *this;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
std::string Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::InspectValue() const
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::stringstream result;
        result << std::quoted(m_value, '\'');
        return result.str();
    }
    if constexpr (std::is_same_v<T, SqlText>)
    {
        std::stringstream result;
        result << std::quoted(m_value.value, '\'');
        return result.str();
    }
    else if constexpr (std::is_same_v<T, SqlDate>)
        return std::format("\'{}\'", m_value.value);
    else if constexpr (std::is_same_v<T, SqlTime>)
        return std::format("\'{}\'", m_value.value);
    else if constexpr (std::is_same_v<T, SqlDateTime>)
        return std::format("\'{}\'", m_value.value);
    else if constexpr (std::is_same_v<T, SqlTimestamp>)
        return std::format("\'{}\'", m_value.value);
    else
        return std::format("{}", m_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
SqlResult<void> Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindInputParameter(
    SQLSMALLINT parameterIndex, SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, m_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
SqlResult<void> Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindOutputColumn(SqlStatement& stmt)
{
    return stmt.BindOutputColumn(TheTableColumnIndex, &m_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          StringLiteral TheColumnName,
          FieldValueRequirement TheRequirement>
SqlResult<void> Field<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindOutputColumn(SQLSMALLINT outputIndex,
                                                                                               SqlStatement& stmt)
{
    return stmt.BindOutputColumn(outputIndex, &m_value);
}

#pragma endregion

#pragma region BelongsTo<> implementation

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>&
BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator=(RecordId modelId) noexcept
{
    SetModified(true);
    m_value = modelId;
    return *this;
}

template <typename ModelType,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
BelongsTo<ModelType, TheColumnIndex, TheForeignKeyName, TheRequirement>& BelongsTo<
    ModelType,
    TheColumnIndex,
    TheForeignKeyName,
    TheRequirement>::operator=(Record<ModelType> const& model) noexcept
{
    SetModified(true);
    m_value = model.Id();
    return *this;
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
std::string BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::InspectValue() const
{
    return std::to_string(m_value.value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
SqlResult<void> BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindInputParameter(
    SQLSMALLINT parameterIndex, SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, m_value.value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
SqlResult<void> BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindOutputColumn(
    SqlStatement& stmt)
{
    return stmt.BindOutputColumn(TheColumnIndex, &m_value.value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
SqlResult<void> BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindOutputColumn(
    SQLSMALLINT outputIndex, SqlStatement& stmt)
{
    return stmt.BindOutputColumn(outputIndex, &m_value.value);
}

#pragma endregion

} // namespace Model
