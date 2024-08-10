#pragma once

// Skatching the surface of a modern C++23 Object Relational Mapping (ORM) layer
//
// We usually know exactly the schema of the database we are working with,
// but we need to be able to address columns with an unknown name but known index,
// and we need to be able to deal with tables of unknown column count.

#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"
#include "SqlStatement.hpp"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

template <size_t N>
struct SqlStringLiteral
{
    constexpr SqlStringLiteral(const char (&str)[N]) noexcept
    {
        std::copy_n(str, N, value);
    }

    char value[N];
};

struct SqlColumnIndex
{
    size_t value;
};

struct SqlModelId
{
    size_t value;
};

// {{{ schema
enum class SqlWhereOperator : uint8_t
{
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    GREATER_THAN,
    LESS_OR_EQUAL,
    GREATER_OR_EQUAL
};

enum class SqlColumnType : uint8_t
{
    UNKNOWN,
    STRING,
    BOOLEAN,
    INTEGER,
    REAL,
    BLOB,
    DATE,
    TIME,
    TIMESTAMP,
};

struct SqlColumnSchema
{
    // std::string name;
    SqlColumnType type = SqlColumnType::UNKNOWN;
    bool isNullable : 1 = false;
    bool isPrimaryKey : 1 = false;
    bool isAutoIncrement : 1 = false;
    bool isUnique : 1 = false;
    bool isInded : 1 = false;
    unsigned columnSize = 0;
};

// Loads and represents a table schema from the database
class SqlTableSchema
{
  public:
    using ColumnList = std::vector<SqlColumnSchema>;
    SqlTableSchema(std::string tableName, ColumnList columns);

    std::string const& TableName() const noexcept;
    SqlColumnSchema const& Columns() const noexcept;

    SqlResult<SqlTableSchema> Load(SqlConnection& connection, std::string tableName);

  private:
    std::string m_tableName;
    std::vector<SqlColumnSchema> m_columns;
};
// }}}

// Forward declarations
template <typename Derived, SqlStringLiteral TableNameLiteral, SqlStringLiteral PrimaryKeyLiteral>
struct SqlModel;

// Base class for all fields in a model.
class SqlModelFieldRegistrable
{
  public:
    virtual ~SqlModelFieldRegistrable() = default;

    virtual void BindOutputColumn(SqlStatement& statement) const = 0;
};

class SqlModelRelation
{
  public:
    virtual ~SqlModelRelation() = default;
};

// Used to get fields registered to their model.
class SqlModelFieldRegistry
{
  public:
    // clang-format off
    void RegisterField(SqlModelFieldRegistrable& field) noexcept { m_fields.push_back(&field); }
    void RegisterRelation(SqlModelRelation& relation) noexcept { m_relations.push_back(&relation); }
    SqlModelFieldRegistrable const& GetField(SqlColumnIndex index) const noexcept { return *m_fields[index.value]; }
    SqlModelFieldRegistrable& GetField(SqlColumnIndex index) noexcept { return *m_fields[index.value]; }
    // clang-format on

  private:
    std::vector<SqlModelFieldRegistrable*> m_fields;
    std::vector<SqlModelRelation*> m_relations;
};

// Represents a single column in a table.
//
// The column name, index, and type are known at compile time.
// If either name or index are not known at compile time, leave them at their default values,
// but at least one of them msut be known.
template <typename T = SqlVariant, std::size_t TheTableColumnIndex = 0, SqlStringLiteral TheColumnName = "">
class SqlModelField: public SqlModelFieldRegistrable
{
  public:
    static constexpr inline std::size_t ColumnIndex = TheTableColumnIndex;
    static constexpr inline std::string_view ColumnName = TheColumnName;

    explicit SqlModelField(SqlModelFieldRegistry& registry)
    {
        registry.RegisterField(*this);
    }

    // clang-format off

    T& Data() noexcept { return m_value; }
    T const& Data() const noexcept { return m_value; }
    void SetData(T&& value) { m_value = std::move(value); }
    void SetNull() { m_value = T {}; }

    SqlModelField& operator=(T&& value) noexcept;

    T& operator*() noexcept;
    T const& operator*() const noexcept;

    // clang-format on

    void BindOutputColumn(SqlStatement& statement) const override;

  private:
    T m_value {};
};

// Represents a column in a table that is a foreign key to another table.
template <typename Model, std::size_t TheColumnIndex, SqlStringLiteral TheForeignKeyName>
class SqlModelBelongsTo: public SqlModelFieldRegistrable
{
  public:
    explicit SqlModelBelongsTo(SqlModelFieldRegistry& registry)
    {
        registry.RegisterField(*this);
    }

    SqlModelBelongsTo& operator=(SqlModelId modelId) noexcept;
    //SqlModelBelongsTo& operator=(SqlModel<Model> const& model) noexcept;

    Model* operator->() noexcept;
    Model& operator*() noexcept;

    constexpr static inline std::size_t ColumnIndex { TheColumnIndex };
    constexpr static inline std::string_view ColumnName { TheForeignKeyName.value };

    void BindOutputColumn(SqlStatement& statement) const override;
};

template <typename Model>
struct HasOne: public SqlModelRelation
{
    Model* operator->() noexcept;
    Model& operator*() noexcept;

    explicit HasOne(SqlModelFieldRegistry& registry)
    {
        (void) registry; // registry.RegisterRelation(*this);
    }
};

template <typename Model>
struct HasMany: public SqlModelRelation
{
    size_t Count() const noexcept;
    std::vector<Model> All() const noexcept;

    explicit HasMany(SqlModelFieldRegistry& registry)
    {
        registry.RegisterRelation(*this);
    }
};

#if 0
// This SQL query builder is used to build the queries for the SqlModel.
namespace SqlQueryBuilder
{

class Builder
{
  public:
    Builder() = default;

    Builder& Select(const std::string& columns) &&;
    Builder& Update(const std::string& columns) &&;
    Builder& Delete() &&;
    Builder& From(const std::string& table) &&;
    Builder& Where(const std::string& column, const SqlVariant& value, SqlWhereOperator whereOperator) &&;
    Builder& OrderBy(const std::string& column, bool ascending = true) &&;
    Builder& Limit(size_t limit) &&;
    Builder& Offset(size_t offset) &&;

    std::string Build() const&&;

    class ColumnsBuilder
    {
    };
    class BuilderStart
    {
        Columns
    };

  private:
    std::string m_query;
};

SqlQueryBuilder::Selecting Select();

} // namespace SqlQueryBuilder

inline auto testBuildQuery()
{
    return SqlQueryBuilder::Builder {}.Select("name").From("persons").Where("id", 1);
}
#endif

template <typename Derived, SqlStringLiteral TableNameLiteral, SqlStringLiteral PrimaryKeyLiteral = "id">
struct SqlModel: public SqlModelFieldRegistry
{
  public:
    constexpr static inline std::string_view TableName = TableNameLiteral;
    constexpr static inline std::string_view PrimaryKeyName = PrimaryKeyLiteral;

    SqlModel();
    ~SqlModel() = default;

    SqlResult<void> Save();
    SqlResult<SqlModelId> Create();
    SqlResult<void> Delete();

    static SqlResult<SqlModel> First() noexcept;

    static SqlResult<SqlModel> Last() noexcept;

    template <typename ColumnName, typename T>
    static SqlResult<SqlModel> FindBy(ColumnName const& columnName, T const& value) noexcept;

    static SqlResult<std::vector<SqlModel>> All() noexcept;

    static SqlResult<std::vector<SqlModel>> Find(SqlModelId id) noexcept;

    static SqlResult<std::vector<SqlModel>> Where(SqlColumnIndex columnIndex,
                                                  const SqlVariant& value,
                                                  SqlWhereOperator whereOperator = SqlWhereOperator::EQUAL) noexcept;

    static SqlResult<std::vector<SqlModel>> Where(const std::string& columnName,
                                                  const SqlVariant& value,
                                                  SqlWhereOperator whereOperator = SqlWhereOperator::EQUAL) noexcept;

    static SqlResult<void> CreateTable() noexcept;
};

template <typename Derived, SqlStringLiteral TableNameLiteral, SqlStringLiteral PrimaryKeyLiteral>
SqlModel<Derived, TableNameLiteral, PrimaryKeyLiteral>::SqlModel()
{
}

template <typename Derived, SqlStringLiteral TableNameLiteral, SqlStringLiteral PrimaryKeyLiteral>
SqlResult<void> SqlModel<Derived, TableNameLiteral, PrimaryKeyLiteral>::CreateTable() noexcept
{
    auto model = SqlModel<Derived, TableNameLiteral, PrimaryKeyLiteral>();
    return {};
}

template <typename T, std::size_t TheTableColumnIndex, SqlStringLiteral TheColumnName>
void SqlModelField<T, TheTableColumnIndex, TheColumnName>::BindOutputColumn(SqlStatement& statement) const
{
    (void) statement; // TODO
}

template <typename Model, std::size_t TheColumnIndex, SqlStringLiteral TheForeignKeyName>
void SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName>::BindOutputColumn(SqlStatement& statement) const
{
    (void) statement; // TODO
}

template <typename T, std::size_t TheTableColumnIndex, SqlStringLiteral TheColumnName>
SqlModelField<T, TheTableColumnIndex, TheColumnName>& SqlModelField<T, TheTableColumnIndex, TheColumnName>::operator=(
    T&& value) noexcept
{
    m_value = std::move(value);
    return *this;
}
