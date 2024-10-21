// SPDX-License-Identifier: MIT
#pragma once

#include "../../SqlComposedQuery.hpp"
#include "../AbstractRecord.hpp"
#include "../StringLiteral.hpp"

namespace Model
{

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
class HasManyThrough
{
  public:
    explicit HasManyThrough(AbstractRecord& record);
    explicit HasManyThrough(AbstractRecord& record, HasManyThrough&& other) noexcept;

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] size_t Count() const;

    std::vector<TargetRecord>& All();

    template <typename Callback>
    void Each(Callback&& callback);

    TargetRecord& At(size_t index);
    TargetRecord const& At(size_t index) const;
    TargetRecord& operator[](size_t index);
    TargetRecord const& operator[](size_t index) const;

    [[nodiscard]] bool IsLoaded() const noexcept;
    void Load();
    void Reload();

  private:
    void RequireLoaded() const;

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
size_t HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::Count() const
{
    if (IsLoaded())
        return m_models.size();

    auto const targetRecord = TargetRecord();
    auto const throughRecordMeta = ThroughRecord(); // TODO: eliminate instances, allowing direct access to meta info

    return TargetRecord::Join(throughRecordMeta.TableName(),
                              LeftKeyName.value,
                              SqlQualifiedTableColumnName(targetRecord.TableName(), targetRecord.PrimaryKeyName()))
        .Join(m_record->TableName(),
              m_record->PrimaryKeyName(),
              SqlQualifiedTableColumnName(throughRecordMeta.TableName(), RightKeyName.value))
        .Where(SqlQualifiedTableColumnName(m_record->TableName(), m_record->PrimaryKeyName()), *m_record->Id())
        .Count();
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
inline std::vector<TargetRecord>& HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::All()
{
    RequireLoaded();
    return m_models;
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
TargetRecord& HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::At(size_t index)
{
    RequireLoaded();
    return m_models.at(index);
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
TargetRecord const& HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::At(size_t index) const
{
    RequireLoaded();
    return m_models.at(index);
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
TargetRecord& HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::operator[](size_t index)
{
    RequireLoaded();
    return m_models[index];
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
TargetRecord const& HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::operator[](
    size_t index) const
{
    RequireLoaded();
    return m_models[index];
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
template <typename Callback>
inline void HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::Each(Callback&& callback)
{
    if (IsLoaded())
    {
        for (auto& model: m_models)
            callback(model);
    }
    else
    {
        auto const targetRecord = TargetRecord();
        auto const throughRecordMeta = ThroughRecord();

        TargetRecord::Join(throughRecordMeta.TableName(),
                           LeftKeyName.value,
                           SqlQualifiedTableColumnName(targetRecord.TableName(), targetRecord.PrimaryKeyName()))
            .Join(m_record->TableName(),
                  m_record->PrimaryKeyName(),
                  SqlQualifiedTableColumnName(throughRecordMeta.TableName(), RightKeyName.value))
            .Where(SqlQualifiedTableColumnName(m_record->TableName(), m_record->PrimaryKeyName()), *m_record->Id())
            .Each(callback);
    }
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
inline bool HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::IsLoaded() const noexcept
{
    return m_loaded;
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
void HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::Load()
{
    if (m_loaded)
        return;

    auto const targetRecord = TargetRecord();
    auto const throughRecordMeta = ThroughRecord();

    m_models =
        TargetRecord::Join(throughRecordMeta.TableName(),
                           LeftKeyName.value,
                           SqlQualifiedTableColumnName(targetRecord.TableName(), targetRecord.PrimaryKeyName()))
            .Join(m_record->TableName(),
                  m_record->PrimaryKeyName(),
                  SqlQualifiedTableColumnName(throughRecordMeta.TableName(), RightKeyName.value))
            .Where(SqlQualifiedTableColumnName(m_record->TableName(), m_record->PrimaryKeyName()), *m_record->Id())
            .All();

    m_loaded = true;
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
void HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::Reload()
{
    m_loaded = false;
    m_models.clear();
    Load();
}

template <typename TargetRecord, StringLiteral LeftKeyName, typename ThroughRecord, StringLiteral RightKeyName>
void HasManyThrough<TargetRecord, LeftKeyName, ThroughRecord, RightKeyName>::RequireLoaded() const
{
    if (!IsLoaded())
        const_cast<HasManyThrough*>(this)->Load();
}

// }}}

} // namespace Model
