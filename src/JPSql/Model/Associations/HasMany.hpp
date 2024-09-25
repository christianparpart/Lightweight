// SPDX-License-Identifier: MIT
#pragma once

#include "../../SqlError.hpp"
#include "../AbstractRecord.hpp"
#include "../Logger.hpp"
#include "../StringLiteral.hpp"

#include <vector>

namespace Model
{

template <typename OtherRecord, StringLiteral ForeignKeyName>
class HasMany
{
  public:
    explicit HasMany(AbstractRecord& parent);
    HasMany(AbstractRecord& record, HasMany&& other) noexcept;

    [[nodiscard]] bool IsEmpty() const;
    [[nodiscard]] size_t Count() const;

    std::vector<OtherRecord>& All();

    OtherRecord& At(size_t index);
    OtherRecord& operator[](size_t index);

    [[nodiscard]] bool IsLoaded() const noexcept;
    void Load();
    void Reload();

  private:
    bool RequireLoaded();

    bool m_loaded = false;
    AbstractRecord* m_record;
    std::vector<OtherRecord> m_models;
};

// {{{ HasMany<> implementation

template <typename OtherRecord, StringLiteral ForeignKeyName>
HasMany<OtherRecord, ForeignKeyName>::HasMany(AbstractRecord& parent):
    m_record { &parent }
{
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
HasMany<OtherRecord, ForeignKeyName>::HasMany(AbstractRecord& record, HasMany&& other) noexcept:
    m_loaded { other.m_loaded },
    m_record { &record },
    m_models { std::move(other.m_models) }
{
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
void HasMany<OtherRecord, ForeignKeyName>::Load()
{
    if (m_loaded)
        return;

    m_models = OtherRecord::Where(*ForeignKeyName, m_record->Id()).All();
    m_loaded = true;
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
void HasMany<OtherRecord, ForeignKeyName>::Reload()
{
    m_loaded = false;
    m_models.clear();
    return Load();
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
bool HasMany<OtherRecord, ForeignKeyName>::IsEmpty() const
{
    return Count() == 0;
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
size_t HasMany<OtherRecord, ForeignKeyName>::Count() const
{
    if (m_loaded)
        return m_models.size();

    SqlStatement stmt;

    auto const sqlQueryString = std::format(
        "SELECT COUNT(*) FROM {} WHERE {} = {}", OtherRecord().TableName(), *ForeignKeyName, *m_record->Id());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});
    return stmt.ExecuteDirectScalar<size_t>(sqlQueryString).value();
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline std::vector<OtherRecord>& HasMany<OtherRecord, ForeignKeyName>::All()
{
    RequireLoaded();
    return m_models;
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline OtherRecord& HasMany<OtherRecord, ForeignKeyName>::At(size_t index)
{
    RequireLoaded();
    return m_models.at(index);
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline OtherRecord& HasMany<OtherRecord, ForeignKeyName>::operator[](size_t index)
{
    RequireLoaded();
    return m_models[index];
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline bool HasMany<OtherRecord, ForeignKeyName>::IsLoaded() const noexcept
{
    return m_loaded;
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline bool HasMany<OtherRecord, ForeignKeyName>::RequireLoaded()
{
    if (!m_loaded)
        Load();

    return m_loaded;
}

// }}}

} // namespace Model
