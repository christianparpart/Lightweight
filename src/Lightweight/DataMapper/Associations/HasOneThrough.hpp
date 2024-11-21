// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "../../SqlComposedQuery.hpp"
#include "../AbstractRecord.hpp"
#include "../StringLiteral.hpp"

#include <memory>

namespace Model
{

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
class HasOneThrough
{
  public:
    explicit HasOneThrough(AbstractRecord& record);
    HasOneThrough(AbstractRecord& record, HasOneThrough&& other) noexcept;

    OtherRecord& operator*();
    OtherRecord* operator->();

    [[nodiscard]] bool IsLoaded() const noexcept;
    void Load();
    void Reload();

  private:
    AbstractRecord* m_record;
    std::shared_ptr<OtherRecord> m_otherRecord;
};

// {{{ inlines

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::HasOneThrough(AbstractRecord& record):
    m_record { &record }
{
}

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::HasOneThrough(AbstractRecord& record,
                                                                         HasOneThrough&& other) noexcept:
    m_record { &record },
    m_otherRecord { std::move(other.m_otherRecord) }
{
}

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
OtherRecord& HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::operator*()
{
    if (!m_otherRecord)
        Load();

    return *m_otherRecord;
}

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
OtherRecord* HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::operator->()
{
    if (!m_otherRecord)
        Load();

    return &*m_otherRecord;
}

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
bool HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::IsLoaded() const noexcept
{
    return m_otherRecord.get();
}

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
void HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::Load()
{
    if (IsLoaded())
        return;

    auto result =
        OtherRecord::template Join<ThroughRecord, ForeignKeyName>()
            .Where(SqlQualifiedTableColumnName(OtherRecord().TableName(), ForeignKeyName.value), m_record->Id().value)
            .First();

    if (!result.has_value())
    {
        SqlLogger::GetLogger().OnWarning(std::format("No data found on table {} for {} = {}",
                                                     OtherRecord().TableName(),
                                                     ForeignKeyName.value,
                                                     m_record->Id().value));
        return;
    }

    m_otherRecord = std::make_shared<OtherRecord>(std::move(result.value()));
}

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
void HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::Reload()
{
    m_otherRecord.reset();
    return Load();
}

// }}}

} // namespace Model
