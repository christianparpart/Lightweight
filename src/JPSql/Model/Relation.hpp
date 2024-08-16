#pragma once

namespace Model
{

class Relation
{
  public:
    virtual ~Relation() = default;
};

template <typename OtherRecord, StringLiteral ForeignKeyName>
class HasMany: public Relation
{
  public:
    explicit HasMany(AbstractRecord& parent):
        m_record { &parent }
    {
        parent.RegisterRelation(*this);
    }

    HasMany(AbstractRecord& record, HasMany&& other) noexcept:
        m_loaded { other.m_loaded },
        m_record { &record },
        m_models { std::move(other.m_models) }
    {
    }

    SqlResult<bool> IsEmpty() const noexcept;
    SqlResult<size_t> Count() const noexcept;

    std::vector<OtherRecord>& All() noexcept;

    bool IsLoaded() const noexcept;
    SqlResult<void> Load();
    SqlResult<void> Reload();

    OtherRecord& At(size_t index) noexcept;
    OtherRecord& operator[](size_t index) noexcept;

  private:
    bool RequireLoaded();

    bool m_loaded = false;
    AbstractRecord* m_record;
    std::vector<OtherRecord> m_models;
};

#pragma region HasMany<> implementation

template <typename OtherRecord, StringLiteral ForeignKeyName>
SqlResult<void> HasMany<OtherRecord, ForeignKeyName>::Load()
{
    if (m_loaded)
        return {};

    return OtherRecord::Where(*ForeignKeyName, m_record->Id()).and_then([&](auto&& models) -> SqlResult<void> {
        m_models = std::move(models);
        m_loaded = true;
        return {};
    });
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
    if (m_loaded)
        return m_models.empty();

    auto const sqlQueryString = std::format("SELECT COUNT(*) FROM {}", OtherRecord().TableName());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    SqlStatement stmt;
    return stmt.ExecuteDirect(sqlQueryString)
        .and_then([&] { return stmt.FetchRow(); })
        .and_then([&]() -> SqlResult<bool> { return stmt.GetColumn<unsigned long long>(1) == 0; });
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

#pragma endregion

} // namespace Model
