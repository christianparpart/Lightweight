#pragma once

// Skatching the surface of a modern C++23 ORM layer
//
// We usually know exactly the schema of the database we are working with,
// but we need to be able to address columns with an unknown name but known index,
// and we need to be able to deal with tables of unknown column count.

#include "SqlConnection.hpp"
#include "SqlDataBinder.hpp"

#include <string>
#include <string_view>
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

struct SqlColumnSchema
{
    std::string name;
    std::string type;
    bool isNullable;
    bool isPrimaryKey;
    bool isAutoIncrement;
    bool isUnique;
    bool isInded;
};

class SqlTableSchema
{
  public:
    SqlTableSchema(std::string_view tableName, SqlConnection* connection);
    ~SqlTableSchema();

    size_t ColumnCount() const noexcept;
    const SqlColumnSchema& GetColumn(size_t index) const;

  private:
    std::vector<SqlColumnSchema> m_columns;
};

template <typename T = SqlVariant, std::size_t TableColumnIndex = 0, SqlStringLiteral ColumnName = "">
class SqlModelField
{
  public:
    SqlModelField(const std::string& name, T&& value):
        m_name(name),
        m_value(std::move(value))
    {
    }

  private:
    std::string m_name;
    T m_value;
};

struct SqlColumnIndex
{
    size_t value;
};

struct SqlModelId
{
    size_t value;
};

template <typename T>
struct Relation
{
    std::string name;
    std::string type;
};

template <typename T>
struct HasMany: public Relation<T>
{
    std::vector<T> value;
};

template <typename T>
struct HasOne: public Relation<T>
{
    T value;
};

template <typename T, SqlStringLiteral TheForeignKeyName, SqlStringLiteral TheTableName>
struct SqlForeignKey
{
    T* foreignModel {};

    constexpr static inline std::string_view ForeignKeyName { TheForeignKeyName.value };
    constexpr static inline std::string_view TableName { TheTableName.value };
};

template <typename Derived, SqlStringLiteral TableName>
struct SqlModel
{
  public:
    SqlModel(SqlConnection* connection, const SqlTableSchema& schema);
    ~SqlModel();

    SqlResult<void> Save();
    SqlResult<SqlModelId> Create();
    SqlResult<void> Delete();

    template <typename T>
    void SetValue(const std::string& name, const T& value)
    {
        m_value[index.value] = std::forward<T>(value);
    }

    template <typename T>
    void GetValue(SqlColumnIndex index, T&& value)
    {
        m_value[index.value] = std::forward<T>(value);
    }

  private:
    SqlConnection* m_connection;
    const SqlTableSchema& m_schema;
    std::vector<SqlModelField<SqlVariant>> m_values;
};

namespace Example
{

struct Person;
struct Phone: SqlModel<Phone, "phones">
{
    SqlModelField<std::string, 1, "name"> number;
    SqlModelField<std::string, 1, "type"> type;
    SqlForeignKey<Person, "person"> person;
};

struct Person: SqlModel<Person, "persons">
{
    SqlModelField<std::string> name;
    HasOne<Company> company;
    HasMany<Phone> phones;
};

struct Company: SqlModel<Company, "companies">
{
    SqlModelField<std::string> name;
    HasMany<Person> employees;
};

#if 0
inline void testMain()
{
    SqlConnection connection;
    SqlTableSchema schema("persons", &connection);

    Phone phone(&connection, schema);
    phone.SetValue("name", "555-1234");
    phone.SetValue("type", "mobile");
    phone.Create();

    Person person(&connection, schema);
    person.SetValue("name", "John Doe");
    person.Create();

    person.phones.value.push_back(phone);
    person.Create();

    Company company(&connection, schema);
    company.SetValue("name", "ACME Inc.");
    company.Create();

    person.company.value = company;
    person.Save();
}
#endif

} // namespace Example
