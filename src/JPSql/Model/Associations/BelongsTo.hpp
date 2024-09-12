// SPDX-License-Identifier: MIT
#pragma once

#include "../../SqlStatement.hpp"
#include "../AbstractField.hpp"
#include "../AbstractRecord.hpp"
#include "../ColumnType.hpp"
#include "../RecordId.hpp"
#include "../StringLiteral.hpp"

#include <cassert>
#include <memory>
#include <string_view>

namespace Model
{

template <typename T>
struct Record;

// Represents a column in a table that is a foreign key to another table.
template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement = FieldValueRequirement::NOT_NULL>
class BelongsTo final: public AbstractField
{
  public:
    constexpr static inline SQLSMALLINT ColumnIndex { TheColumnIndex };
    constexpr static inline std::string_view ColumnName { TheForeignKeyName.value };

    explicit BelongsTo(AbstractRecord& record);
    BelongsTo(BelongsTo const& other);
    explicit BelongsTo(AbstractRecord& record, BelongsTo&& other);
    BelongsTo& operator=(RecordId modelId);
    BelongsTo& operator=(OtherRecord const& model);
    ~BelongsTo() = default;

    OtherRecord* operator->();
    OtherRecord& operator*();

    std::string SqlConstraintSpecifier() const override;

    std::string InspectValue() const override;
    void BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    void BindOutputColumn(SqlStatement& stmt) override;
    void BindOutputColumn(SQLSMALLINT index, SqlStatement& stmt) override;
    void LoadValueFrom(AbstractField& other) override;

    auto operator<=>(BelongsTo const& other) const noexcept;

    template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
    bool operator==(BelongsTo<U, I, N, R> const& other) const noexcept;

    template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
    bool operator!=(BelongsTo<U, I, N, R> const& other) const noexcept;

    void Load() noexcept;

  private:
    void RequireLoaded();

    RecordId m_value {};
    std::shared_ptr<OtherRecord> m_otherRecord;

    // We decided to use shared_ptr here, because we do not want to require to know the size of the OtherRecord
    // at declaration time.
};

// {{{ BelongsTo<> implementation

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BelongsTo(AbstractRecord& record):
    AbstractField {
        record, TheColumnIndex, TheForeignKeyName.value, ColumnTypeOf<RecordId>, TheRequirement,
    }
{
    record.RegisterField(*this);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BelongsTo(BelongsTo const& other):
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

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BelongsTo(AbstractRecord& record,
                                                                               BelongsTo&& other):
    AbstractField { std::move(static_cast<AbstractField&&>(other)) },
    m_value { std::move(other.m_value) }
{
    record.RegisterField(*this);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>&
BelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator=(RecordId modelId)
{
    SetModified(true);
    m_value = modelId;
    return *this;
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>&
BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator=(OtherRecord const& model)
{
    SetModified(true);
    m_value = model.Id();
    return *this;
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline OtherRecord* BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator->()
{
    RequireLoaded();
    return &*m_otherRecord;
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline OtherRecord& BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator*()
{
    RequireLoaded();
    return *m_otherRecord;
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
std::string BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::SqlConstraintSpecifier() const
{
    auto const otherRecord = OtherRecord {};
    // TODO: Move the syntax into SqlTraits, as a parametrized member function
    return std::format("FOREIGN KEY ({}) REFERENCES {}({}) ON DELETE CASCADE",
                       ColumnName,
                       otherRecord.TableName(),
                       otherRecord.PrimaryKeyName());
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline std::string BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::InspectValue() const
{
    return std::to_string(m_value.value);
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline void BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindInputParameter(
    SQLSMALLINT parameterIndex, SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, m_value.value);
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline void BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindOutputColumn(
    SqlStatement& stmt)
{
    return stmt.BindOutputColumn(TheColumnIndex, &m_value.value);
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline void BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindOutputColumn(
    SQLSMALLINT outputIndex, SqlStatement& stmt)
{
    return stmt.BindOutputColumn(outputIndex, &m_value.value);
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
void BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::LoadValueFrom(AbstractField& other)
{
    assert(Type() == other.Type());
    m_value = std::move(static_cast<BelongsTo&>(other).m_value);
    m_otherRecord.reset();
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline auto BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator<=>(
    BelongsTo const& other) const noexcept
{
    return m_value <=> other.m_value;
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
inline bool BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator==(
    BelongsTo<U, I, N, R> const& other) const noexcept
{
    return m_value == other.m_value;
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
template <typename U, SQLSMALLINT I, StringLiteral N, FieldValueRequirement R>
inline bool BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator!=(
    BelongsTo<U, I, N, R> const& other) const noexcept
{
    return m_value == other.m_value;
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
void BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::Load() noexcept
{
    if (m_otherRecord)
        return;

    auto otherRecord = OtherRecord::Find(m_value);
    if (otherRecord.has_value())
        m_otherRecord = std::make_shared<OtherRecord>(std::move(otherRecord.value()));
}

template <typename OtherRecord,
          SQLSMALLINT TheColumnIndex,
          StringLiteral TheForeignKeyName,
          FieldValueRequirement TheRequirement>
inline void BelongsTo<OtherRecord, TheColumnIndex, TheForeignKeyName, TheRequirement>::RequireLoaded()
{
    if (!m_otherRecord)
    {
        Load();
        if (!m_otherRecord)
            throw std::runtime_error("BelongsTo::RequireLoaded(): Record not found");
    }
}

// }}}

} // namespace Model
