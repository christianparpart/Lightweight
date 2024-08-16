#pragma once

namespace Model
{

class Relation
{
  public:
    virtual ~Relation() = default;
};

// Represents an association of another Model with a foreign key to this model.
template <typename Model>
class HasOne final: public Relation
{
  public:
    explicit HasOne(AbstractRecord& registry)
    {
        registry.RegisterRelation(*this);
    }

    Model* operator->() noexcept; // TODO
    Model& operator*() noexcept;  // TODO

    bool Load() noexcept;           // TODO
    bool IsLoaded() const noexcept; // TODO

  private:
    std::optional<Model> m_model;
};

template <typename Model, StringLiteral ForeignKeyName>
class HasMany: public Relation
{
  public:
    explicit HasMany(AbstractRecord& model):
        m_model { &model }
    {
        model.RegisterRelation(*this);
    }

    HasMany(AbstractRecord& model, HasMany&& other) noexcept:
        m_loaded { other.m_loaded },
        m_model { other.m_model },
        m_models { std::move(other.m_models) }
    {
        other.m_model = nullptr;
    }

    SqlResult<bool> IsEmpty() const noexcept;
    SqlResult<size_t> Count() const noexcept;

    std::vector<Model>& All() noexcept;

    bool IsLoaded() const noexcept;
    SqlResult<void> Load();
    SqlResult<void> Reload();

    Model& At(size_t index) noexcept;
    Model& operator[](size_t index) noexcept;

  private:
    bool RequireLoaded();

    bool m_loaded = false;
    AbstractRecord* m_model;
    std::vector<Model> m_models;
};

#pragma region HasMany<> implementation

template <typename Model, StringLiteral ForeignKeyName>
SqlResult<void> HasMany<Model, ForeignKeyName>::Load()
{
    if (m_loaded)
        return {};

    return Model::Where(*ForeignKeyName, *m_model->Id())
        .and_then([&](auto&& models) -> SqlResult<void> {
            m_models = std::move(models);
            m_loaded = true;
            return {};
        });
}

template <typename Model, StringLiteral ForeignKeyName>
SqlResult<void> HasMany<Model, ForeignKeyName>::Reload()
{
    m_loaded = false;
    m_models.clear();
    return Load();
}

template <typename Model, StringLiteral ForeignKeyName>
SqlResult<bool> HasMany<Model, ForeignKeyName>::IsEmpty() const noexcept
{
    if (m_loaded)
        return m_models.empty();

    auto const sqlQueryString = std::format("SELECT COUNT(*) FROM {}", Model().TableName());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    SqlStatement stmt;
    return stmt.ExecuteDirect(sqlQueryString)
        .and_then([&] { return stmt.FetchRow(); })
        .and_then([&]() -> SqlResult<bool> { return stmt.GetColumn<unsigned long long>(1) == 0; });
}

template <typename Model, StringLiteral ForeignKeyName>
SqlResult<size_t> HasMany<Model, ForeignKeyName>::Count() const noexcept
{
    if (m_loaded)
        return m_models.size();

    SqlStatement stmt;

    auto const sqlQueryString =
        std::format("SELECT COUNT(*) FROM {} WHERE {} = {}", Model().TableName(), *ForeignKeyName, *m_model->Id());
    auto const scopedModelSqlLogger = detail::SqlScopedModelQueryLogger(sqlQueryString, {});

    return stmt.Prepare(sqlQueryString)
        .and_then([&] { return stmt.Execute(); })
        .and_then([&] { return stmt.FetchRow(); })
        .and_then([&] { return stmt.GetColumn<size_t>(1); });
}

template <typename Model, StringLiteral ForeignKeyName>
inline std::vector<Model>& HasMany<Model, ForeignKeyName>::All() noexcept
{
    RequireLoaded();
    return m_models;
}

template <typename Model, StringLiteral ForeignKeyName>
inline Model& HasMany<Model, ForeignKeyName>::At(size_t index) noexcept
{
    RequireLoaded();
    return m_models.at(index);
}

template <typename Model, StringLiteral ForeignKeyName>
inline Model& HasMany<Model, ForeignKeyName>::operator[](size_t index) noexcept
{
    RequireLoaded();
    return m_models[index];
}

template <typename Model, StringLiteral ForeignKeyName>
inline bool HasMany<Model, ForeignKeyName>::IsLoaded() const noexcept
{
    return m_loaded;
}

#pragma endregion

} // namespace Model
