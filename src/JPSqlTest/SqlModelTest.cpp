// TODO: add std::formatter for SqlModel<T>
// TODO: add std::formatter for SqlResult<T>

#include "../JPSql/SqlConnection.hpp"
#include "../JPSql/SqlModel.hpp"
#include "JPSqlTestUtils.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <print>

using namespace std::string_view_literals;

struct Person;
struct Company;
struct Phone;
struct Job;

namespace
{

class SqlTestFixture
{
  public:
    SqlTestFixture()
    {
        // (customize or enable if needed)
        // SqlLogger::SetLogger(SqlLogger::TraceLogger());
        SqlConnection::SetDefaultConnectInfo(TestSqlConnectionString);
    }

    ~SqlTestFixture()
    {
        // Don't linger pooled idle connections into the next test case
        SqlConnection::KillAllIdle();
    }
};
} // namespace

int main(int argc, char const* argv[])
{
    return Catch::Session().run(argc, argv);
}

struct Author;
struct Book;

struct Book: SqlModel<Book>
{
    SqlModelField<std::string, 2, "title"> title;
    SqlModelField<std::string, 3, "isbn"> isbn;
    SqlModelBelongsTo<Author, 4, "author_id"> author;

    Book():
        SqlModel { "books" },
        title { *this },
        isbn { *this },
        author { *this }
    {
    }

    Book(Book&& other) noexcept:
        SqlModel { std::move(other) },
        title { *this, std::move(other.title) },
        isbn { *this, std::move(other.isbn) },
        author { *this, std::move(other.author) }
    {
    }
};

struct Author: SqlModel<Author>
{
    SqlModelField<std::string, 2, "name"> name;
    HasMany<Book, "author_id"> books;

    Author():
        SqlModel { "authors" },
        name { *this },
        books { *this }
    {
    }
};

TEST_CASE_METHOD(SqlTestFixture, "Model.Create", "[model]")
{
    REQUIRE(Author::CreateTable());
    REQUIRE(Book::CreateTable());

    Author author;
    author.name = "Bjarne Stroustrup";
    REQUIRE(author.Save());
    REQUIRE(author.Id().value == 1);
    REQUIRE(author.books.Size().value() == 0);

    Book book1;
    book1.title = "The C++ Programming Language";
    book1.isbn = "978-0-321-56384-2";
    book1.author = author;
    REQUIRE(book1.Save());
    REQUIRE(book1.Id().value == 1);
    REQUIRE(Book::Count().value() == 1);
    REQUIRE(author.books.Size().value() == 1);

    Book book2;
    book2.title = "A Tour of C++";
    book2.isbn = "978-0-321-958310";
    book2.author = author;
    REQUIRE(book2.Save());
    REQUIRE(book2.Id().value == 2);
    REQUIRE(Book::Count().value() == 2);
    REQUIRE(author.books.Size().value() == 2);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Load", "[model]")
{
    REQUIRE(Author::CreateTable());
    REQUIRE(Book::CreateTable());

    Author author;
    author.name = "Bjarne Stroustrup";
    REQUIRE(author.Save());

    Book book;
    book.title = "The C++ Programming Language";
    book.isbn = "978-0-321-56384-2";
    book.author = author;
    REQUIRE(book.Save());

    Book bookLoaded;
    bookLoaded.Load(book.Id());
    INFO("Book: " << book.Inspect());
    CHECK(bookLoaded.Id() == book.Id());
    CHECK(bookLoaded.title == book.title);
    CHECK(bookLoaded.isbn == book.isbn);
    CHECK(bookLoaded.author == book.author);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Find", "[model]")
{
    REQUIRE(Author::CreateTable());
    REQUIRE(Book::CreateTable());

    Author author;
    author.name = "Bjarne Stroustrup";
    REQUIRE(author.Save());

    Book book;
    book.title = "The C++ Programming Language";
    book.isbn = "978-0-321-56384-2";
    book.author = author;
    REQUIRE(book.Save());

    Book bookLoaded = Book::Find(book.Id()).value();
    INFO("Book: " << book.Inspect());
    CHECK(bookLoaded.Id() == book.Id());     // primary key
    CHECK(bookLoaded.title == book.title);   // SqlModelField<>
    CHECK(bookLoaded.isbn == book.isbn);     // SqlModelField<>
    CHECK(bookLoaded.author == book.author); // BelongsTo<>
}

#if 0
TEST_CASE_METHOD(SqlTestFixture, "Updating", "[model]")
{
    REQUIRE(Author::CreateTable());
    REQUIRE(Book::CreateTable());

    Author author;
    author.name = "Bjarne Stroustrup";
    REQUIRE(author.Save());

    Book book1;
    book1.title = "The C++ Programming Language";
    book1.isbn = "978-0-321-56384-2";
    book1.author = author;
    REQUIRE(book1.Save());

    Book book2;
    book2.title = "A Tour of C++";
    book2.isbn = "978-0-321-958310";
    book2.author = author;
    REQUIRE(book2.Save());

    book2.title = "A Tour of C++ (2nd Edition)";
    book2.isbn = "978-0-321-958310-2";
    REQUIRE(book2.Save());

    Book book2Loaded = Book::Find(book2.Id()).value();
    INFO("Book 1        : " << book1.Inspect());
    INFO("Book 2        : " << book2.Inspect());
    INFO("Book 2 loaded : " << book2Loaded.Inspect());
    CHECK(book2Loaded.Id() == book2.Id());
    CHECK(book2Loaded.title == book2.title);
    CHECK(book2Loaded.isbn == book2.isbn);
    //REQUIRE(book2Loaded.author.Id() == author.Id());
}
#endif

#if 0
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

    Person(Person const& other):
        SqlModel { "persons" },
        firstName { *this, other.firstName },
        lastName { *this, other.lastName },
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
    #if 1
    auto const databaseFilePath = argc == 2 && std::string_view(argv[1]) == "--memory"
                                      ? std::filesystem::path("file::memory")
                                      : std::filesystem::path(argv[0]).parent_path() / "ModelTest.sqlite";
    #else
    (void) argc;
    (void) argv;
    auto const databaseFilePath = std::filesystem::path("C:/source/test.sqlite");
    #endif

    SqlConnection::SetDefaultConnectInfo(SqlConnectionString {
        .connectionString = std::format("DRIVER={};Database={}", TestSqlDriver, databaseFilePath.string()),
    });

    std::print("-- CREATING TABLES:\n\n{}\n",
               CreateSqlTablesString<Company, Person, Phone, Job>(SqlConnection().ServerType()));

    CreateSqlTables<Company, Person, Phone, Job>().or_else(FatalError);

    Person susi;
    susi.firstName = "Susi";
    susi.lastName = "Hanni-Nanni-Bunny";
    susi.Save();

    Person person;
    person.firstName = "John";
    person.lastName = "Doe";
    person.Save().or_else(FatalError);
    std::println("Person: {}", person.Inspect());

    Phone phone;
    phone.number = "555-1234";
    phone.type = "mobile";
    phone.owner = person;
    phone.Save().or_else(FatalError);
    std::println("Phone: {}", phone.Inspect());

    Job job;
    job.title = "Software Developer";
    job.salary = 50'000;
    job.startDate = SqlDate::Today();
    job.person = person;
    job.isCurrent = true;
    job.Save().or_else(FatalError);
    std::println("Job Initial: {}", job.Inspect());

    job.salary = 60'000;
    job.Save().or_else(FatalError); // only the salary field is updated
    std::println("Job Updated: {}", job.Inspect());

    std::println("persons in database: {}", Person::Count().value_or(0));

    auto allPersons = Person::All().value();
    std::println("all persons count: {}", allPersons.size());
    for (auto const& person: allPersons)
        std::println("Person: {}", person.Inspect());

    return 0;
}
#endif
