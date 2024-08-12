#pragma once

#include "../SqlStatement.hpp"
#include "ColumnType.hpp"
#include "ModelId.hpp"
#include "StringLiteral.hpp"

#include <string_view>

namespace Model
{

enum class SqlFieldValueRequirement : uint8_t
{
    NULLABLE,
    NOT_NULL,
};

constexpr inline SqlFieldValueRequirement SqlNullable = SqlFieldValueRequirement::NULLABLE;
constexpr inline SqlFieldValueRequirement SqlNotNullable = SqlFieldValueRequirement::NULLABLE;

// Base class for all fields in a model.
class SqlModelFieldBase
{
  public:
    SqlModelFieldBase(SqlModelBase& model,
                      SQLSMALLINT index,
                      std::string_view name,
                      SqlColumnType type,
                      SqlFieldValueRequirement requirement):
        m_model { &model },
        m_index { index },
        m_name { name },
        m_type { type },
        m_requirement { requirement }
    {
    }

    SqlModelFieldBase() = delete;
    SqlModelFieldBase(SqlModelFieldBase&&) = default;
    SqlModelFieldBase& operator=(SqlModelFieldBase&&) = default;
    SqlModelFieldBase(SqlModelFieldBase const&) = delete;
    SqlModelFieldBase& operator=(SqlModelFieldBase const&) = delete;
    virtual ~SqlModelFieldBase() = default;

    virtual std::string InspectValue() const = 0;
    virtual SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const = 0;
    virtual SqlResult<void> BindOutputColumn(SqlStatement& stmt) = 0;

    // clang-format off
    SqlModelBase& Model() noexcept { return *m_model; }
    SqlModelBase const& Model() const noexcept { return *m_model; }
    bool IsModified() const noexcept { return m_modified; }
    void SetModified(bool value) noexcept { m_modified = value; }
    SQLSMALLINT Index() const noexcept { return m_index; }
    std::string_view Name() const noexcept { return m_name; }
    SqlColumnType Type() const noexcept { return m_type; }
    bool IsNullable() const noexcept { return m_requirement == SqlFieldValueRequirement::NULLABLE; }
    bool IsRequired() const noexcept { return m_requirement == SqlFieldValueRequirement::NOT_NULL; }
    // clang-format on

  private:
    SqlModelBase* m_model;
    SQLSMALLINT m_index;
    std::string_view m_name;
    SqlColumnType m_type;
    SqlFieldValueRequirement m_requirement;
    bool m_modified = false;
};

// Represents a single column in a table.
//
// The column name, index, and type are known at compile time.
// If either name or index are not known at compile time, leave them at their default values,
// but at least one of them msut be known.
template <typename T,
          SQLSMALLINT TheTableColumnIndex = 0,
          SqlStringLiteral TheColumnName = "",
          SqlFieldValueRequirement TheRequirement = SqlFieldValueRequirement::NOT_NULL>
class SqlModelField final: public SqlModelFieldBase
{
  public:
    explicit SqlModelField(SqlModelBase& registry):
        SqlModelFieldBase {
            registry, TheTableColumnIndex, TheColumnName.value, SqlColumnTypeOf<T>, TheRequirement,
        }
    {
        registry.RegisterField(*this);
    }

    explicit SqlModelField(SqlModelBase& registry, SqlModelField&& field):
        SqlModelFieldBase {
            registry, TheTableColumnIndex, TheColumnName.value, SqlColumnTypeOf<T>, TheRequirement,
        },
        m_value { std::move(field.m_value) }
    {
        registry.RegisterField(*this);
    }

    SqlModelField() = delete;
    SqlModelField(SqlModelField&& other) = delete;
    SqlModelField& operator=(SqlModelField&& other) = delete;
    SqlModelField& operator=(SqlModelField const& other) = delete;
    SqlModelField(SqlModelField const& other) = delete;
    ~SqlModelField() = default;

    // clang-format off

    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    auto operator<=>(SqlModelField<U, I, N, R> const& other) const noexcept { return m_value <=> other.m_value; }

    // We also define the equality and inequality operators explicitly, because <=> from above does not seem to work in MSVC VS 2022.
    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    auto operator==(SqlModelField<U, I, N, R> const& other) const noexcept { return m_value == other.m_value; }

    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    auto operator!=(SqlModelField<U, I, N, R> const& other) const noexcept { return m_value != other.m_value; }

    T const& Value() const noexcept { return m_value; }
    void SetData(T&& value) { SetModified(true); m_value = std::move(value); }
    void SetNull() { SetModified(true); m_value = T {}; }

    SqlModelField& operator=(T&& value) noexcept;

    T& operator*() noexcept { return m_value; }
    T const& operator*() const noexcept { return m_value; }

    // clang-format on

    std::string InspectValue() const override;
    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;

  private:
    T m_value {};
};

// Represents a column in a table that is a foreign key to another table.
template <typename ModelType,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement = SqlFieldValueRequirement::NOT_NULL>
class SqlModelBelongsTo final: public SqlModelFieldBase
{
  public:
    explicit SqlModelBelongsTo(SqlModelBase& registry):
        SqlModelFieldBase {
            registry, TheColumnIndex, TheForeignKeyName.value, SqlColumnTypeOf<SqlModelId>, TheRequirement,
        }
    {
        registry.RegisterField(*this);
    }

    explicit SqlModelBelongsTo(SqlModelBase& registry, SqlModelBelongsTo&& other):
        SqlModelFieldBase { std::move(other) },
        m_value { other.m_value }
    {
        registry.RegisterField(*this);
    }

    SqlModelBelongsTo& operator=(SqlModelId modelId) noexcept;
    SqlModelBelongsTo& operator=(SqlModel<ModelType> const& model) noexcept;

    ModelType* operator->() noexcept; // TODO
    ModelType& operator*() noexcept;  // TODO

    constexpr static inline SQLSMALLINT ColumnIndex { TheColumnIndex };
    constexpr static inline std::string_view ColumnName { TheForeignKeyName.value };

    std::string InspectValue() const override;
    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;

    auto operator<=>(SqlModelBelongsTo const& other) const noexcept
    {
        return m_value <=> other.m_value;
    }

    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    bool operator==(SqlModelBelongsTo<U, I, N, R> const& other) const noexcept
    {
        return m_value == other.m_value;
    }

    template <typename U, SQLSMALLINT I, SqlStringLiteral N, SqlFieldValueRequirement R>
    bool operator!=(SqlModelBelongsTo<U, I, N, R> const& other) const noexcept
    {
        return m_value == other.m_value;
    }

  private:
    SqlModelId m_value {};
};

#pragma region SqlModelField<> implementation

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>&
SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::operator=(T&& value) noexcept
{
    SetModified(true);
    m_value = std::move(value);
    return *this;
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
std::string SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::InspectValue() const
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        std::stringstream result;
        result << std::quoted(m_value);
        return std::move(result.str());
    }
    else if constexpr (std::is_same_v<T, SqlDate>)
        return std::format("\"{}\"", m_value.value);
    else if constexpr (std::is_same_v<T, SqlTime>)
        return std::format("\"{}\"", m_value.value);
    else if constexpr (std::is_same_v<T, SqlTimestamp>)
        return std::format("\"{}\"", m_value.value);
    else
        return std::format("{}", m_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindInputParameter(
    SQLSMALLINT parameterIndex, SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, m_value);
}

template <typename T,
          SQLSMALLINT TheTableColumnIndex,
          SqlStringLiteral TheColumnName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelField<T, TheTableColumnIndex, TheColumnName, TheRequirement>::BindOutputColumn(
    SqlStatement& stmt)
{
    return stmt.BindOutputColumn(TheTableColumnIndex, &m_value);
}

#pragma endregion

#pragma region BelongsTo<> implementation

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>&
SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::operator=(SqlModelId modelId) noexcept
{
    SetModified(true);
    m_value = modelId;
    return *this;
}

template <typename ModelType,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlModelBelongsTo<ModelType, TheColumnIndex, TheForeignKeyName, TheRequirement>& SqlModelBelongsTo<
    ModelType,
    TheColumnIndex,
    TheForeignKeyName,
    TheRequirement>::operator=(SqlModel<ModelType> const& model) noexcept
{
    SetModified(true);
    m_value = model.Id();
    return *this;
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
std::string SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::InspectValue() const
{
    return std::to_string(m_value.value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindInputParameter(
    SQLSMALLINT parameterIndex, SqlStatement& stmt) const
{
    return stmt.BindInputParameter(parameterIndex, m_value.value);
}

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlResult<void> SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>::BindOutputColumn(
    SqlStatement& stmt)
{
    return stmt.BindOutputColumn(TheColumnIndex, &m_value.value);
}

#pragma endregion

} // namespace Model
