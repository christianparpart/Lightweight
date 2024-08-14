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

    void Load() noexcept;           // TODO
    bool IsLoaded() const noexcept; // TODO

  private:
    std::optional<Model> m_model;
};

template <typename Model, StringLiteral ForeignKeyName>
class HasMany: public Relation
{
  public:
    SqlResult<size_t> Count() const noexcept;

    std::vector<Model>& All() noexcept
    {
        RequireLoaded();
        return m_models;
    }

    explicit HasMany(AbstractRecord& model):
        m_model { &model }
    {
        model.RegisterRelation(*this);
    }

    bool IsLoaded() const noexcept
    {
        return m_loaded;
    }

    void Load();
    void Reload();

    bool IsEmpty() const noexcept
    {
        RequireLoaded();
        return m_models.empty();
    }

    SqlResult<size_t> Size() const noexcept
    {
        if (m_loaded)
            return m_models.size();

        SqlStatement stmt;

        auto const sqlQuery =
            std::format("SELECT COUNT(*) FROM {} WHERE {} = {}", Model().TableName(), *ForeignKeyName, *m_model->Id());

        return stmt.Prepare(sqlQuery)
            .and_then([&] { return stmt.Execute(); })
            .and_then([&] { return stmt.FetchRow(); })
            .and_then([&] { return stmt.GetColumn<size_t>(1); });
    }

    Model& At(size_t index) noexcept
    {
        RequireLoaded();
        return m_models.at(index);
    }

    Model& operator[](size_t index) noexcept
    {
        RequireLoaded();
        return m_models[index];
    }

  private:
    bool RequireLoaded();

    bool m_loaded = false;
    AbstractRecord* m_model;
    std::vector<Model> m_models;
};

#pragma region HasMany<> implementation

template <typename Model, StringLiteral ForeignKeyName>
SqlResult<size_t> HasMany<Model, ForeignKeyName>::Count() const noexcept
{
    if (!m_models.empty())
        return m_models.size();

    auto stmt = SqlStatement {};
    auto const conceptModel = Model();
    auto result = stmt.Prepare(
        std::format("SELECT COUNT(*) FROM {} WHERE {} = ?", conceptModel.TableName(), conceptModel.PrimaryKeyName()));
    if (!stmt.Execute())
        return 0;
    if (!stmt.FetchRow())
        return 0;
    return stmt.GetColumn<size_t>(1);
}

#pragma endregion

} // namespace Model
