// SPDX-License-Identifier: MIT
#pragma once

#include "../../SqlComposedQuery.hpp"
#include "../../SqlError.hpp"
#include "../../SqlStatement.hpp"
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

    bool IsLoaded() const noexcept;
    SqlResult<void> Load();
    SqlResult<void> Reload();

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
SqlResult<void> HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::Load()
{
    if (IsLoaded())
        return {};

    // SELECT :other_record.*
    //     FROM :other_record
    //     INNER JOIN :through_record ON :other_record.:foreign_key_name = :through_record.id
    //     WHERE :other_record.id = :record.id

    auto otherRecord = std::make_shared<OtherRecord>();
    auto const metaThroughRecord = ThroughRecord();

    auto stmt = SqlStatement {};

    auto const sqlQueryString =
        SqlQueryBuilder::From(otherRecord->TableName())
            .Select(otherRecord->AllFieldNames())
            .InnerJoin(metaThroughRecord.TableName(),
                       metaThroughRecord.PrimaryKeyName(),
                       otherRecord->TableName(),
                       ForeignKeyName.value)
            .Where(SqlQualifiedTableColumnName(otherRecord->TableName(), ForeignKeyName.value), SqlQueryWildcard())
            .First()
            .ToSql(stmt.Connection().QueryFormatter());

    auto scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    if (auto result = stmt.Prepare(sqlQueryString); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.BindInputParameter(1, m_record->Id()); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    if (auto result = stmt.BindOutputColumn(1, &otherRecord->MutableId().value); !result)
        return std::unexpected { result.error() };

    for (AbstractField* field: otherRecord->AllFields())
        if (auto result = field->BindOutputColumn(stmt); !result)
            return std::unexpected { result.error() };

    if (auto result = stmt.FetchRow(); !result)
        return std::unexpected { result.error() };

    m_otherRecord = std::move(otherRecord);

    return {};
}

template <typename OtherRecord, StringLiteral ForeignKeyName, typename ThroughRecord>
SqlResult<void> HasOneThrough<OtherRecord, ForeignKeyName, ThroughRecord>::Reload()
{
    m_otherRecord.reset();
    return Load();
}

// }}}

} // namespace Model
