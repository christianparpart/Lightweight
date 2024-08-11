#pragma once

// Skatching the surface of a modern C++23 Object Relational Mapping (ORM) layer
//
// We usually know exactly the schema of the database we are working with,
// but we need to be able to address columns with an unknown name but known index,
// and we need to be able to deal with tables of unknown column count.

#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"
#include "SqlStatement.hpp"

#include <algorithm>
#include <ranges>
#include <sstream>
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

// {{{ SqlModelId
struct SqlModelId
{
    size_t value;
};

template <>
struct SqlDataBinder<SqlModelId>
{
    static SQLRETURN InputParameter(SQLHSTMT stmt, SQLSMALLINT column, SqlModelId const& value)
    {
        return SqlDataBinder<decltype(value.value)>::InputParameter(stmt, column, value.value);
    }

    static SQLRETURN OutputColumn(
        SQLHSTMT stmt, SQLSMALLINT column, SqlModelId* result, SQLLEN* indicator, SqlDataBinderCallback& cb)
    {
        return SqlDataBinder<decltype(result->value)>::OutputColumn(stmt, column, &result->value, indicator, cb);
    }

    static SQLRETURN GetColumn(SQLHSTMT stmt, SQLSMALLINT column, SqlModelId* result, SQLLEN* indicator)
    {
        return SqlDataBinder<decltype(result->value)>::GetColumn(stmt, column, &result->value, indicator);
    }
};
// }}}

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

constexpr std::string_view SqlColumnTypeName(SqlColumnType value) noexcept
{
    switch (value)
    {
        case SqlColumnType::STRING:
            return "TEXT";
        case SqlColumnType::BOOLEAN:
            return "BOOLEAN";
        case SqlColumnType::INTEGER:
            return "INTEGER";
        case SqlColumnType::REAL:
            return "REAL";
        case SqlColumnType::BLOB:
            return "BLOB";
        case SqlColumnType::DATE:
            return "DATE";
        case SqlColumnType::TIME:
            return "TIME";
        case SqlColumnType::TIMESTAMP:
            return "TIMESTAMP";
    }
    return "UNKNOWN";
}

namespace detail
{
template <typename>
struct SqlColumnTypeOf;

// clang-format off
template <> struct SqlColumnTypeOf<std::string> { static constexpr SqlColumnType value = SqlColumnType::STRING; };
template <> struct SqlColumnTypeOf<bool> { static constexpr SqlColumnType value = SqlColumnType::BOOLEAN; };
template <> struct SqlColumnTypeOf<int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned int> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<unsigned long long> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
template <> struct SqlColumnTypeOf<float> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct SqlColumnTypeOf<double> { static constexpr SqlColumnType value = SqlColumnType::REAL; };
template <> struct SqlColumnTypeOf<SqlDate> { static constexpr SqlColumnType value = SqlColumnType::DATE; };
template <> struct SqlColumnTypeOf<SqlTime> { static constexpr SqlColumnType value = SqlColumnType::TIME; };
template <> struct SqlColumnTypeOf<SqlTimestamp> { static constexpr SqlColumnType value = SqlColumnType::TIMESTAMP; };
template <> struct SqlColumnTypeOf<SqlModelId> { static constexpr SqlColumnType value = SqlColumnType::INTEGER; };
// clang-format on
} // namespace detail

template <typename T>
constexpr SqlColumnType SqlColumnTypeOf = detail::SqlColumnTypeOf<T>::value;

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
template <typename Derived>
struct SqlModel;

template <typename... Models>
SqlResult<void> CreateSqlTables();

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
    SqlModelFieldBase(SQLSMALLINT index,
                      std::string_view name,
                      SqlColumnType type,
                      SqlFieldValueRequirement requirement):
        m_index { index },
        m_name { name },
        m_type { type },
        m_requirement { requirement }
    {
    }

    virtual ~SqlModelFieldBase() = default;

    // Must be implemented by SqlModelField<T> and SqlModelBelongsTo<Model> to bind the field's value to a statement.
    virtual SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const = 0;
    virtual SqlResult<void> BindOutputColumn(SqlStatement& stmt) = 0;

    // clang-format off
    bool IsModified() const noexcept { return m_modified; }
    void SetModified(bool value) noexcept { m_modified = value; }
    SQLSMALLINT Index() const noexcept { return m_index; }
    std::string_view Name() const noexcept { return m_name; }
    SqlColumnType Type() const noexcept { return m_type; }
    bool IsNullable() const noexcept { return m_requirement == SqlFieldValueRequirement::NULLABLE; }
    bool IsRequired() const noexcept { return m_requirement == SqlFieldValueRequirement::NOT_NULL; }
    // clang-format on

  private:
    SQLSMALLINT m_index;
    std::string_view m_name;
    SqlColumnType m_type;
    SqlFieldValueRequirement m_requirement;
    bool m_modified = false;
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
    void RegisterField(SqlModelFieldBase& field) noexcept { m_fields.push_back(&field); }
    void RegisterRelation(SqlModelRelation& relation) noexcept { m_relations.push_back(&relation); }
    SqlModelFieldBase const& GetField(SqlColumnIndex index) const noexcept { return *m_fields[index.value]; }
    SqlModelFieldBase& GetField(SqlColumnIndex index) noexcept { return *m_fields[index.value]; }
    // clang-format on

    void SortFieldsByIndex() noexcept
    {
        std::sort(m_fields.begin(), m_fields.end(), [](auto a, auto b) { return a->Index() < b->Index(); });
    }

  protected:
    std::vector<SqlModelFieldBase*> m_fields;
    std::vector<SqlModelRelation*> m_relations;
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
class SqlModelField: public SqlModelFieldBase
{
  public:
    explicit SqlModelField(SqlModelFieldRegistry& registry):
        SqlModelFieldBase {
            TheTableColumnIndex,
            TheColumnName.value,
            SqlColumnTypeOf<T>,
            TheRequirement,
        }
    {
        registry.RegisterField(*this);
    }

    // clang-format off

    T const& Value() const noexcept { return m_value; }
    void SetData(T&& value) { SetModified(true); m_value = std::move(value); }
    void SetNull() { SetModified(true); m_value = T {}; }

    SqlModelField& operator=(T&& value) noexcept;

    T& operator*() noexcept;
    T const& operator*() const noexcept;

    // clang-format on

    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;

  private:
    T m_value {};
};

// Represents a column in a table that is a foreign key to another table.
template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement = SqlFieldValueRequirement::NOT_NULL>
class SqlModelBelongsTo: public SqlModelFieldBase
{
  public:
    explicit SqlModelBelongsTo(SqlModelFieldRegistry& registry):
        SqlModelFieldBase {
            TheColumnIndex,
            TheForeignKeyName.value,
            SqlColumnTypeOf<SqlModelId>,
            TheRequirement,
        }
    {
        registry.RegisterField(*this);
    }

    SqlModelBelongsTo& operator=(SqlModelId modelId) noexcept;
    SqlModelBelongsTo& operator=(SqlModel<Model> const& model) noexcept;

    Model* operator->() noexcept;
    Model& operator*() noexcept;

    constexpr static inline SQLSMALLINT ColumnIndex { TheColumnIndex };
    constexpr static inline std::string_view ColumnName { TheForeignKeyName.value };

    SqlResult<void> BindInputParameter(SQLSMALLINT parameterIndex, SqlStatement& stmt) const override;
    SqlResult<void> BindOutputColumn(SqlStatement& stmt) override;

  private:
    SqlModelId m_value {};
};

template <typename Model>
struct HasOne: public SqlModelRelation
{
    Model* operator->() noexcept;
    Model& operator*() noexcept;

    explicit HasOne(SqlModelFieldRegistry& registry)
    {
        registry.RegisterRelation(*this);
    }
};

template <typename Model>
class HasMany: public SqlModelRelation
{
  public:
    size_t Count() const noexcept;

    std::vector<Model>& All() noexcept
    {
        RequireLoaded();
        return m_models;
    }

    explicit HasMany(SqlModelFieldRegistry& registry)
    {
        registry.RegisterRelation(*this);
    }

    bool IsLoaded() const noexcept
    {
        return m_loaded;
    }

    void Load();

    bool IsEmpty() const noexcept
    {
        RequireLoaded();
        return m_models.empty();
    }

    size_t Size() const noexcept
    {
        // TODO: consider simply doing: SELECt COUNT(*) FROM table WHERE foreign_key = ?
        RequireLoaded();
        return m_models.size();
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
    std::vector<Model> m_models;
};

#if 0 // Builder API
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

template <typename Derived>
struct SqlModel: public SqlModelFieldRegistry
{
    std::string_view tableName;      // Should not be modified, but we want to allow move semantics
    std::string_view primaryKeyName; // Should not be modified, but we want to allow move semantics
    SqlModelId id;

    std::string_view TableName() const noexcept;
    std::string_view PrimaryKeyName() const noexcept;

    // Creates (or recreates a copy of) the model in the database.
    SqlResult<SqlModelId> Create();

    // Reads the model from the database by given model ID.
    SqlResult<void> Read(SqlModelId id);

    // Updates the model in the database.
    SqlResult<void> Update();

    // Creates or updates the model in the database, depending on whether it already exists.
    SqlResult<void> Save();

    // Deletes the model from the database.
    SqlResult<void> Destroy();

    // Updates all models with the given changes in the modelChanges model.
    static SqlResult<void> UpdateAll(SqlModel<Derived> const& modelChanges) noexcept;

    // Retrieves the first model from the database (ordered by ID ASC).
    static SqlResult<SqlModel> First() noexcept;

    // Retrieves the last model from the database (ordered by ID ASC).
    static SqlResult<SqlModel> Last() noexcept;

    template <typename ColumnName, typename T>
    static SqlResult<SqlModel> FindBy(ColumnName const& columnName, T const& value) noexcept;

    // Retrieves all models of this kind from the database.
    static SqlResult<std::vector<SqlModel>> All() noexcept;

    // Retrieves the model with the given ID from the database.
    static SqlResult<SqlModel> Find(SqlModelId id) noexcept;

    static SqlResult<std::vector<SqlModel>> Where(SqlColumnIndex columnIndex,
                                                  const SqlVariant& value,
                                                  SqlWhereOperator whereOperator = SqlWhereOperator::EQUAL) noexcept;

    static SqlResult<std::vector<SqlModel>> Where(const std::string& columnName,
                                                  const SqlVariant& value,
                                                  SqlWhereOperator whereOperator = SqlWhereOperator::EQUAL) noexcept;

    // Returns the SQL string to create the table for this model.
    static std::string CreateTableString(SqlServerType serverType) noexcept;

    // Creates the table for this model in the database.
    static SqlResult<void> CreateTable() noexcept;

  protected:
    explicit SqlModel(std::string_view tableName, std::string_view primaryKey = "id");
};

// TODO: Design a model schema registry, such that an instantiation of a model for
//       regular data use is as efficient as possible.
class SqlModelSchemaRegistry
{
  public:
    template <typename Model>
    void RegisterModel()
    {
        // TODO: m_modelSchemas.emplace_back(SqlModel<Model>(*this));
    }

  private:
    // std::vector<SqlModelSchema> m_modelSchemas;
};

template <typename Derived>
SqlModel<Derived>::SqlModel(std::string_view tableName, std::string_view primaryKey):
    tableName { tableName },
    primaryKeyName { primaryKey },
    id {}
{
}

template <typename Derived>
std::string SqlModel<Derived>::CreateTableString(SqlServerType serverType) noexcept
{
    SqlTraits const& traits = GetSqlTraits(serverType); // TODO: take server type from connection
    std::stringstream sql;
    auto model = Derived();
    model.SortFieldsByIndex();

    // TODO: verify that field indices are unique, contiguous, and start at 1
    // TODO: verify that the primary key is the first field
    // TODO: verify that the primary key is not nullable

    sql << "CREATE TABLE IF NOT EXISTS " << model.tableName << " (\n";

    sql << "    " << model.primaryKeyName << " " << traits.PrimaryKeyAutoIncrement << ",\n";

    for (auto const* field: model.m_fields)
    {
        sql << "    " << field->Name() << " " << SqlColumnTypeName(field->Type());
        if (field->IsNullable())
            sql << " NULL";
        else
            sql << " NOT NULL";
        if (field != model.m_fields.back())
            sql << ",";
        sql << "\n";
    }

    sql << ");\n";

    return std::move(sql.str());
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::CreateTable() noexcept
{
    auto stmt = SqlStatement {};
    return stmt.ExecuteDirect(CreateTableString(stmt.Connection().ServerType()));
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
    SetModified(true);
    return stmt.BindOutputColumn(TheTableColumnIndex, &m_value);
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
    return {};
    (void) stmt; // TODO: return stmt.BindOutputColumn(TheColumnIndex, &m_value.value);
}

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

template <typename Model,
          SQLSMALLINT TheColumnIndex,
          SqlStringLiteral TheForeignKeyName,
          SqlFieldValueRequirement TheRequirement>
SqlModelBelongsTo<Model, TheColumnIndex, TheForeignKeyName, TheRequirement>& SqlModelBelongsTo<
    Model,
    TheColumnIndex,
    TheForeignKeyName,
    TheRequirement>::operator=(SqlModel<Model> const& model) noexcept
{
    SetModified(true);
    m_value = model.id;
    return *this;
}

template <typename Derived>
SqlResult<SqlModelId> SqlModel<Derived>::Create()
{
    auto const requiredFieldCount =
        std::ranges::count_if(m_fields, [](auto const* field) { return field->IsRequired(); });

    auto stmt = SqlStatement {};

    std::string sqlColumnsString = "";
    std::string sqlValuesString = "";
    for (auto const* field: m_fields)
    {
        if (field->IsRequired() && !field->IsModified())
            return std::unexpected { SqlError::FAILURE }; // TODO: return SqlError::MODEL_REQUIRED_FIELD_NOT_GIVEN;

        if (!sqlColumnsString.empty())
        {
            sqlColumnsString += ", ";
            sqlValuesString += ", ";
        }

        sqlColumnsString += field->Name(); // TODO: quote column name
        sqlValuesString += "?";
    }

    auto const sqlInsertStmtString =
        std::format("INSERT INTO {} ({}) VALUES ({})", tableName, sqlColumnsString, sqlValuesString);

    std::print("Creating model with SQL: {}\n", sqlInsertStmtString);

    if (auto result = stmt.Prepare(sqlInsertStmtString); !result)
        return std::unexpected { result.error() };

    SQLSMALLINT parameterIndex = 1;
    for (auto const* field: m_fields)
        if (field->IsModified())
            if (auto result = field->BindInputParameter(parameterIndex++, stmt); !result)
                return std::unexpected { result.error() };

    if (auto result = stmt.Execute(); !result)
        return std::unexpected { result.error() };

    // Update the model's ID with the last insert ID
    if (auto const result = stmt.LastInsertId(); result)
        id.value = *result;

    return {};
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Save()
{
    if (id.value != 0)
        return Update();

    if (auto result = Create(); !result)
        return std::unexpected { result.error() };

    return {};
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Read(SqlModelId modelId)
{
    auto stmt = SqlStatement {};

    if (auto result = stmt.Prepare("SELECT * FROM {} WHERE {} = {}", tableName, id.Name, modelId.value); !result)
        return result;

    for (auto& field: m_fields)
        if (auto result = field.BindOutputColumn(stmt); !result)
            return result;

    return stmt.Execute();
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Update()
{
    auto stmt = SqlStatement {};
    auto columns = std::string {}; // TODO
#if 0                              // TODO
    if (auto result = stmt.Prepare("UPDATE ? SET {} WHERE {} = ?", tableName, primaryKeyName); !result)
        // TODO: number of "?" must match number of parameters
        return result;

    for (auto const* field: m_fields)
        if (field->IsModified())
            if (auto result = field->BindInputParameter(stmt); !result)
                return result;

    if (auto result = stmt.Execute(); !result)
        return result;

    for (auto& field: m_fields)
        field.ResetModified();
#endif
    return {};
}

template <typename Derived>
SqlResult<void> SqlModel<Derived>::Destroy()
{
    auto stmt = SqlStatement {};
    return stmt.ExecuteDirect(std::format("DELETE FROM {} WHERE {} = {}", tableName, id.Name, id.Value()));
}

template <typename... Models>
std::string CreateSqlTablesString(SqlServerType serverType)
{
    std::string result;
    result = ((Models::CreateTableString(serverType) + "\n") + ...);
    return result;
}

template <typename... Models>
SqlResult<void> CreateSqlTables()
{
    SqlResult<void> result;
    ((result = Models::CreateTable()) && ...);
    return result;
}
