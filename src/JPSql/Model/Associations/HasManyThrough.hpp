// SPDX-License-Identifier: MIT
#pragma once

#include "../../SqlComposedQuery.hpp"
#include "../../SqlError.hpp"
#include "../../SqlStatement.hpp"
#include "../AbstractRecord.hpp"
#include "../Record.hpp"
#include "../StringLiteral.hpp"

namespace Model
{

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
class HasManyThrough
{
  public:
    explicit HasManyThrough(AbstractRecord& record);
    explicit HasManyThrough(AbstractRecord& record, HasManyThrough&& other) noexcept;

    bool IsEmpty() const noexcept;
    size_t Count() const noexcept;

    std::vector<TargetRecord>& All() noexcept;

    TargetRecord& At(size_t index) noexcept;
    TargetRecord& operator[](size_t index) noexcept;

    bool IsLoaded() const noexcept;
    SqlResult<void> Load();
    SqlResult<void> Reload();

  private:
    AbstractRecord* m_record;
    bool m_loaded = false;
    std::vector<TargetRecord> m_models;
};

// {{{ inlines

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::HasManyThrough(AbstractRecord& record):
    m_record { &record }
{
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::HasManyThrough(AbstractRecord& record,
                                                                                       HasManyThrough&& other) noexcept:
    m_record { &record },
    m_loaded { other.m_loaded },
    m_models { std::move(other.m_models) }
{
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
bool HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::IsEmpty() const noexcept
{
    return Count() == 0;
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
size_t HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::Count() const noexcept
{
    if (IsLoaded())
        return m_models.size();

    auto const targetRecord = TargetRecord();
    auto const throughRecordMeta = ThroughRecord();

    auto const sqlQueryString =
        SqlQueryBuilder::From(targetRecord.TableName())
            .InnerJoin(throughRecordMeta.TableName(),
                       SqlQualifiedTableColumnName(throughRecordMeta.TableName(), RightKeyName.value),
                       SqlQualifiedTableColumnName(targetRecord.TableName(), targetRecord.PrimaryKeyName()))
            .InnerJoin(m_record->TableName(),
                       SqlQualifiedTableColumnName(m_record->TableName(), m_record->PrimaryKeyName()),
                       SqlQualifiedTableColumnName(throughRecordMeta.TableName(), RightKeyName.value))
            .Where(SqlQualifiedTableColumnName(m_record->TableName(), m_record->PrimaryKeyName()), SqlQueryWildcard())
            .Count()
            .ToSql(SqlConnection().QueryFormatter());

    auto stmt = SqlStatement {};
    return stmt.ExecuteDirect(sqlQueryString)
        .and_then([&] { return stmt.FetchRow(); })
        .and_then([&] { return stmt.GetColumn<size_t>(1); })
        .value();
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
inline std::vector<TargetRecord>& HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::All() noexcept
{
    if (!IsLoaded())
        Load();

    return m_models;
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
inline bool HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::IsLoaded() const noexcept
{
    return m_loaded;
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
SqlResult<void> HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::Load()
{
    if (m_loaded)
        return {};

    auto const targetRecord = TargetRecord();
    auto const throughRecordMeta = ThroughRecord();

    auto const sqlQueryString =
        SqlQueryBuilder::From(targetRecord.TableName())
            .Select(targetRecord.AllFieldNames(), targetRecord.TableName())
            .InnerJoin(throughRecordMeta.TableName(),
                       LeftKeyName.value,
                       SqlQualifiedTableColumnName(targetRecord.TableName(), targetRecord.PrimaryKeyName()))
            .InnerJoin(m_record->TableName(),
                       m_record->PrimaryKeyName(),
                       SqlQualifiedTableColumnName(throughRecordMeta.TableName(), RightKeyName.value))
            .Where(SqlQualifiedTableColumnName(m_record->TableName(), m_record->PrimaryKeyName()), SqlQueryWildcard())
            .All()
            .ToSql(SqlConnection().QueryFormatter());

    m_models = TargetRecord::Query(sqlQueryString, m_record->Id()).value();
    m_loaded = true;
    return {};
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
SqlResult<void> HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::Reload()
{
    m_loaded = false;
    m_models.clear();
    return Load();
}

// }}}

} // namespace Model
