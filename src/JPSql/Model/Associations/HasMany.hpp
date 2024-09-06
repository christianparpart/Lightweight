// SPDX-License-Identifier: MIT
#pragma once

#include "../../SqlError.hpp"
#include "../AbstractRecord.hpp"
#include "../Logger.hpp"
#include "../StringLiteral.hpp"

#include <memory>
#include <vector>

namespace Model
{

template <typename OtherRecord, StringLiteral ForeignKeyName>
class HasMany
{
  public:
    explicit HasMany(AbstractRecord& parent);
    HasMany(AbstractRecord& record, HasMany&& other) noexcept;

    SqlResult<bool> IsEmpty() const noexcept;
    SqlResult<size_t> Count() const noexcept;

    std::vector<OtherRecord>& All() noexcept;

    OtherRecord& At(size_t index) noexcept;
    OtherRecord& operator[](size_t index) noexcept;

    bool IsLoaded() const noexcept;
    SqlResult<void> Load();
    SqlResult<void> Reload();

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
SqlResult<void> HasMany<OtherRecord, ForeignKeyName>::Load()
{
    if (m_loaded)
        return {};

    m_models = OtherRecord::Where(*ForeignKeyName, m_record->Id()).All();
    m_loaded = true;
    return {};
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
SqlResult<void> HasMany<OtherRecord, ForeignKeyName>::Reload()
{
    m_loaded = false;
    m_models.clear();
    return Load();
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
SqlResult<bool> HasMany<OtherRecord, ForeignKeyName>::IsEmpty() const noexcept
{
    return Count() == 0;
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
SqlResult<size_t> HasMany<OtherRecord, ForeignKeyName>::Count() const noexcept
{
    if (m_loaded)
        return m_models.size();

    SqlStatement stmt;

    auto const sqlQueryString = std::format(
        "SELECT COUNT(*) FROM {} WHERE {} = {}", OtherRecord().TableName(), *ForeignKeyName, *m_record->Id());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    return stmt.Prepare(sqlQueryString)
        .and_then([&] { return stmt.Execute(); })
        .and_then([&] { return stmt.FetchRow(); })
        .and_then([&] { return stmt.GetColumn<size_t>(1); });
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline std::vector<OtherRecord>& HasMany<OtherRecord, ForeignKeyName>::All() noexcept
{
    RequireLoaded();
    return m_models;
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline OtherRecord& HasMany<OtherRecord, ForeignKeyName>::At(size_t index) noexcept
{
    RequireLoaded();
    return m_models.at(index);
}

template <typename OtherRecord, StringLiteral ForeignKeyName>
inline OtherRecord& HasMany<OtherRecord, ForeignKeyName>::operator[](size_t index) noexcept
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
