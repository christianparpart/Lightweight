#include "../JPSql/SqlConnection.hpp"
#include "../JPSql/SqlModel.hpp"

#include <filesystem>
#include <print>

using namespace std::string_view_literals;

struct Person;
struct Company;
struct Phone;
struct Job;

struct Phone: SqlModel<Phone>
{
    SqlModelField<std::string, 4, "name"> number;
    SqlModelField<std::string, 3, "type"> type;
    SqlModelBelongsTo<Person, 2, "owner_id", SqlNullable> owner;

    Phone():
        SqlModel { "phones" },
        number { *this },
        type { *this },
        owner { *this }
    {
    }
};

struct Job: SqlModel<Job>
{
    SqlModelBelongsTo<Person, 2, "person_id"> person;
    SqlModelField<std::string, 3, "title"> title;
    SqlModelField<unsigned, 4, "salary"> salary;
    SqlModelField<SqlDate, 5, "start_date"> startDate;
    SqlModelField<SqlDate, 6, "end_date", SqlNullable> endDate;
    SqlModelField<bool, 7, "is_current"> isCurrent;

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
    SqlModelField<std::string, 2, "first_name"> firstName;
    SqlModelField<std::string, 3, "last_name"> lastName;

    HasMany<Job> jobs;
    HasMany<Phone> phones;

    Person():
        SqlModel { "persons" },
        firstName { *this },
        lastName { *this },
        jobs { *this },
        phones { *this }
    {
    }
};

struct Company: SqlModel<Company>
{
    SqlModelField<std::string, 2, "name"> name;
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

SqlResult<void> FatalError(SqlError ec)
{
    std::print("Fatal error: {}\n", ec);
    std::exit(EXIT_FAILURE);
    std::unreachable();
}

int main(int argc, char const* argv[])
{
    auto const databaseFilePath = argc == 2 && std::string_view(argv[1]) == "--memory"
                                      ? std::filesystem::path("file::memory")
                                      : std::filesystem::path(argv[0]).parent_path() / "ModelTest.sqlite";

    SqlConnection::SetDefaultConnectInfo(SqlConnectionString {
        .connectionString = std::format("DRIVER={};Database={}", TestSqlDriver, databaseFilePath.string()),
    });

    std::print("-- CREATING TABLES:\n\n{}\n",
               CreateSqlTablesString<Company, Person, Phone, Job>(SqlConnection().ServerType()));

    CreateSqlTables<Company, Person, Phone, Job>().or_else(FatalError);

    // TODO: add std::formatter for SqlModel<T>
    // TODO: add std::formatter for SqlResult<T>
    // TODO: optimize HasMany<T>.Size()

    Person person;
    person.firstName = "John";
    person.lastName = "Doe";
    person.Save().or_else(FatalError);
    std::print("Person: {}\n", person.Inspect());

    Phone phone;
    phone.number = "555-1234";
    phone.type = "mobile";
    phone.owner = person;
    phone.Save().or_else(FatalError);
    std::print("Phone: {}\n", phone.Inspect());

    Job job;
    job.title = "Software Developer";
    job.salary = 50'000;
    job.startDate = SqlDate::Today();
    job.person = person;
    job.isCurrent = true;
    job.Save().or_else(FatalError);
    std::print("Job Initial: {}\n", job.Inspect());

    job.salary = 60'000;
    job.Save().or_else(FatalError); // only the salary field is updated
    std::print("Job Updated: {}\n", job.Inspect());

//    auto allPersons = Person::All().value();
//    for (auto const& person: allPersons)
//        std::print("Person: {}\n", person.Inspect());
    /*std::ranges::for_each(
        Person::All().or_else(FatalError).value(),
        [](Person const& p) {
            std::print("Person: {}\n", p.Inspect());
        });*/

    return 0;
}
