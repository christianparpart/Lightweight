// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../../SqlError.hpp"
#include "../AbstractRecord.hpp"
#include "../StringLiteral.hpp"

#include <memory>

namespace Model
{

// Represents a column in a another table that refers to this record.
template <typename OtherRecord, StringLiteral TheForeignKeyName>
class HasOne final
{
  public:
    explicit HasOne(AbstractRecord& record);
    explicit HasOne(AbstractRecord& record, HasOne&& other);

    OtherRecord& operator*();
    OtherRecord* operator->();
    [[nodiscard]] bool IsLoaded() const;

    bool Load();
    void Reload();

  private:
    void RequireLoaded();

    AbstractRecord* m_record;
    std::shared_ptr<OtherRecord> m_otherRecord;

    // We decided to use shared_ptr here, because we do not want to require to know the size of the OtherRecord
    // at declaration time.
};

// {{{ HasOne<> implementation

template <typename OtherRecord, StringLiteral TheForeignKeyName>
HasOne<OtherRecord, TheForeignKeyName>::HasOne(AbstractRecord& record):
    m_record { &record }
{
}

template <typename OtherRecord, StringLiteral TheForeignKeyName>
HasOne<OtherRecord, TheForeignKeyName>::HasOne(AbstractRecord& record, HasOne&& other):
    m_record { &record },
    m_otherRecord { std::move(other.m_otherRecord) }
{
}

template <typename OtherRecord, StringLiteral TheForeignKeyName>
OtherRecord& HasOne<OtherRecord, TheForeignKeyName>::operator*()
{
    RequireLoaded();
    return *m_otherRecord;
}

template <typename OtherRecord, StringLiteral TheForeignKeyName>
OtherRecord* HasOne<OtherRecord, TheForeignKeyName>::operator->()
{
    RequireLoaded();
    return &*m_otherRecord;
}

template <typename OtherRecord, StringLiteral TheForeignKeyName>
bool HasOne<OtherRecord, TheForeignKeyName>::IsLoaded() const
{
    return m_otherRecord.get() != nullptr;
}

template <typename OtherRecord, StringLiteral TheForeignKeyName>
bool HasOne<OtherRecord, TheForeignKeyName>::Load()
{
    if (m_otherRecord)
        return true;

    auto foundRecord = OtherRecord::FindBy(TheForeignKeyName.value, m_record->Id());
    if (!foundRecord)
        return false;

    m_otherRecord = std::make_shared<OtherRecord>(std::move(foundRecord.value()));
    return true;
}

template <typename OtherRecord, StringLiteral TheForeignKeyName>
void HasOne<OtherRecord, TheForeignKeyName>::Reload()
{
    m_otherRecord.reset();
    Load();
}

template <typename OtherRecord, StringLiteral TheForeignKeyName>
void HasOne<OtherRecord, TheForeignKeyName>::RequireLoaded()
{
    if (!m_otherRecord)
        Load();
}

// }}}

} // namespace Model
