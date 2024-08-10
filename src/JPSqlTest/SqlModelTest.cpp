#include "../JPSql/SqlConnection.hpp"
#include "../JPSql/SqlModel.hpp"

using namespace std::string_view_literals;

struct Person;
struct Company;
struct Phone;
struct Job;

struct Phone: SqlModel<Phone>
{
    SqlModelField<std::string, 1, "name"> number;
    SqlModelField<std::string, 2, "type"> type;
    SqlModelBelongsTo<Person, 3, "person_id"> person;

    Phone():
        SqlModel { "phones" },
        number { *this },
        type { *this },
        person { *this }
    {
    }
};

struct Job: SqlModel<Job>
{
    SqlModelBelongsTo<Person, 1, "person_id"> person;
    SqlModelField<std::string, 2, "title"> title;
    SqlModelField<unsigned, 3, "salary"> salary;
    SqlModelField<SqlDate, 4, "start_date"> startDate;
    SqlModelField<SqlDate, 5, "end_date"> endDate;
    SqlModelField<bool, 6, "is_current"> isCurrent;

    Job():
        SqlModel { "jobs" },
        person { *this },
        title { *this },
        salary { *this },
        startDate { *this },
        endDate { *this },
        isCurrent { *this }
    {
    }
};

struct Person: SqlModel<Person>
{
    SqlModelField<std::string, 1, "first_name"> firstName;
    SqlModelField<SqlTrimmedString, 2, "last_name"> lastName;

    HasOne<Company> company;
    HasMany<Job> jobs;
    HasMany<Phone> phones;

    Person():
        SqlModel { "persons" },
        firstName { *this },
        lastName { *this },
        company { *this },
        jobs { *this },
        phones { *this }
    {
    }
};

struct Company: SqlModel<Company>
{
    SqlModelField<std::string> name;
    HasMany<Person> employees;

    Company():
        SqlModel { "companies" },
        name { *this },
        employees { *this }
    {
    }
};

#if defined(_WIN32) || defined(_WIN64)
auto constexpr TestSqlDriver = "SQLite3 ODBC Driver"sv;
#else
auto constexpr TestSqlDriver = "SQLite3"sv;
#endif

auto const TestSqlConnectionString = SqlConnectionString {
    .connectionString = std::format("DRIVER={};Database={}", TestSqlDriver, "file::memory:"),
};

int main(int argc, char const* argv[])
{
    (void) argc;
    (void) argv;

    SqlConnection::SetDefaultConnectInfo(SqlConnectionString {
        .connectionString = std::format("DRIVER={};Database={}", TestSqlDriver, "file::memory:"),
    });

    Company::CreateTable();
    Person::CreateTable();
    Phone::CreateTable();
    Job::CreateTable();

    Person person;
    // person.firstName = "John";
    // person.lastName = "Doe";
    person.Create();

    Phone phone;
    phone.number = "555-1234";
    phone.type = "mobile";
    phone.person = SqlModelId { person.id.Value() };

    return 0;
}

#if 0  // {{{
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
#endif // }}}
