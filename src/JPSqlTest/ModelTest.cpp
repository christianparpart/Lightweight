#include "../JPSql/Model.hpp"
#include "../JPSql/SqlConnection.hpp"
#include "JPSqlTestUtils.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace std::string_view_literals;

struct Person;
struct Company;
struct Phone;
struct Job;

int main(int argc, char const* argv[])
{
    int i = 1;
    for (; i < argc; ++i)
    {
        if (argv[i] == "--trace-model"sv)
            Model::QueryLogger::Set(Model::QueryLogger::StandardLogger());
        else if (argv[i] == "--trace-sql"sv)
            SqlLogger::SetLogger(SqlLogger::TraceLogger());
        else if (argv[i] == "--help"sv || argv[i] == "-h"sv)
        {
            std::println("{} [--trace-model] [--trace-sql] [[--] [Catch2 flags ...]]", argv[0]);
            return EXIT_SUCCESS;
        }
        else if (argv[i] == "--"sv)
        {
            ++i;
            break;
        }
        else
            break;
    }

    if (i < argc)
        argv[i - 1] = argv[0];

    return Catch::Session().run(argc - (i - 1), argv + (i - 1));
}

struct Author;
struct Book;

TEST_CASE_METHOD(SqlTestFixture, "Model.Move", "[model]")
{
    struct MovableRecord: public Model::Record<MovableRecord>
    {
        Model::Field<std::string, 2, "name"> name;

        MovableRecord():
            Record { "movables" },
            name { *this }
        {
        }

        MovableRecord(MovableRecord&& other):
            Record { std::move(other) },
            name { *this, std::move(other.name) }
        {
        }
    };

    // Ensure move constructor is working as expected.
    // Inspect() touches the most internal data structures, so we use this call to verify.

    REQUIRE(MovableRecord::CreateTable());

    MovableRecord record;
    record.name = "Foxy Fox";
    record.Save();
    auto const originalText = record.Inspect();
    INFO("Original: " << originalText);

    MovableRecord movedRecord(std::move(record));
    auto const movedText = movedRecord.Inspect();
    REQUIRE(movedText == originalText);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Field: SqlTrimmedString", "[model]")
{
    struct TrimmedStringRecord: Model::Record<TrimmedStringRecord>
    {
        Model::Field<SqlTrimmedString, 2, "name"> name;

        TrimmedStringRecord():
            Record { "trimmed_strings" },
            name { *this }
        {
        }

        TrimmedStringRecord(TrimmedStringRecord&& other):
            Record { std::move(other) },
            name { *this, std::move(other.name) }
        {
        }
    };

    REQUIRE(TrimmedStringRecord::CreateTable());

    TrimmedStringRecord record;
    record.name = SqlTrimmedString { "  Hello, World!  " };
    REQUIRE(record.Save());

    REQUIRE(record.Reload()); // Ensure we fetch name from the database and got trimmed on fetch.

    CHECK(record.name == SqlTrimmedString { "  Hello, World!" });
}

struct Author: Model::Record<Author>
{
    Model::Field<std::string, 2, "name"> name;
    Model::HasMany<Book, "author_id"> books;

    Author():
        Record { "authors" },
        name { *this },
        books { *this }
    {
    }

    Author(Author&& other):
        Record { std::move(other) },
        name { *this, std::move(other.name) },
        books { *this, std::move(other.books) }
    {
    }
};

struct Book: Model::Record<Book>
{
    Model::Field<std::string, 2, "title"> title;
    Model::Field<std::string, 3, "isbn"> isbn;
    Model::BelongsTo<Author, 4, "author_id"> author;

    Book():
        Record { "books", "id" },
        title { *this },
        isbn { *this },
        author { *this }
    {
    }

    Book(Book&& other):
        Record { std::move(other) },
        title { *this, std::move(other.title) },
        isbn { *this, std::move(other.isbn) },
        author { *this, std::move(other.author) }
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
    REQUIRE(author.books.Count().value() == 0);

    Book book1;
    book1.title = "The C++ Programming Language";
    book1.isbn = "978-0-321-56384-2";
    book1.author = author;
    REQUIRE(book1.Save());
    REQUIRE(book1.Id().value == 1);
    REQUIRE(Book::Count().value() == 1);
    REQUIRE(author.books.Count().value() == 1);

    Book book2;
    book2.title = "A Tour of C++";
    book2.isbn = "978-0-321-958310";
    book2.author = author;
    REQUIRE(book2.Save());
    REQUIRE(book2.Id().value == 2);
    REQUIRE(Book::Count().value() == 2);
    REQUIRE(author.books.Count().value() == 2);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Load", "[model]")
{
    REQUIRE(Model::CreateSqlTables<Author, Book>());

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
    REQUIRE(Model::CreateSqlTables<Author, Book>());

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
    CHECK(bookLoaded.title == book.title);   // Field<>
    CHECK(bookLoaded.isbn == book.isbn);     // Field<>
    CHECK(bookLoaded.author == book.author); // BelongsTo<>
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Update", "[model]")
{
    REQUIRE(Model::CreateSqlTables<Author, Book>());

    Author author;
    author.name = "Bjarne Stroustrup";
    REQUIRE(author.Save());

    Book book;
    book.title = "The C++ Programming Language";
    book.isbn = "978-0-321-56384-2";
    book.author = author;
    REQUIRE(book.Save());

    book.isbn = "978-0-321-958310";
    REQUIRE(book.Save());

    Book bookRead = Book::Find(book.Id()).value();
    CHECK(bookRead.Id() == book.Id());
    CHECK(bookRead.title == book.title);
    CHECK(bookRead.isbn == book.isbn);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Destroy", "[model]")
{
    REQUIRE(Author::CreateTable());

    Author author1;
    author1.name = "Bjarne Stroustrup";
    REQUIRE(author1.Save());
    REQUIRE(Author::Count() == 1);

    Author author2;
    author2.name = "John Doe";
    REQUIRE(author2.Save());
    REQUIRE(Author::Count() == 2);

    REQUIRE(author1.Destroy());
    REQUIRE(Author::Count() == 1);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.All", "[model]")
{
    REQUIRE(Author::CreateTable());

    Author author1;
    author1.name = "Bjarne Stroustrup";
    REQUIRE(author1.Save());

    Author author2;
    author2.name = "John Doe";
    REQUIRE(author2.Save());

    Author author3;
    author3.name = "Some very long name";
    REQUIRE(author3.Save());

    Author author4;
    author4.name = "Shorty";
    REQUIRE(author4.Save());

    auto authors = Author::All().value();
    REQUIRE(authors.size() == 4);
    CHECK(authors[0].name == author1.name);
    CHECK(authors[1].name == author2.name);
    CHECK(authors[2].name == author3.name);
    CHECK(authors[3].name == author4.name);
}

struct ColumnTypesRecord: Model::Record<ColumnTypesRecord>
{
    Model::Field<std::string, 2, "the_string"> stringColumn;
    Model::Field<SqlText, 3, "the_text"> textColumn;

    ColumnTypesRecord():
        Record { "column_types" },
        stringColumn { *this },
        textColumn { *this }
    {
    }

    ColumnTypesRecord(ColumnTypesRecord&& other):
        Record { std::move(other) },
        stringColumn { *this, std::move(other.stringColumn) },
        textColumn { *this, std::move(other.textColumn) }
    {
    }
};

TEST_CASE_METHOD(SqlTestFixture, "Model.ColumnTypes", "[model]")
{
    REQUIRE(ColumnTypesRecord::CreateTable());

    ColumnTypesRecord record;
    record.stringColumn = "Hello";
    record.textColumn = SqlText { ", World!" };
    REQUIRE(record.Save());

    ColumnTypesRecord record2 = ColumnTypesRecord::Find(record.Id()).value();
    CHECK(record2.stringColumn == record.stringColumn);
    CHECK(record2.textColumn == record.textColumn);
}

struct Employee: Model::Record<Employee>
{
    Model::Field<std::string, 2, "name"> name;
    Model::Field<bool, 3, "is_senior"> isSenior;

    Employee():
        Record { "employees" },
        name { *this },
        isSenior { *this }
    {
    }

    Employee(Employee&& other):
        Record { std::move(other) },
        name { *this, std::move(other.name) },
        isSenior { *this, std::move(other.isSenior) }
    {
    }
};

TEST_CASE_METHOD(SqlTestFixture, "Model.Where", "[model]")
{
    REQUIRE(Employee::CreateTable());

    Employee employee1;
    employee1.name = "John Doe";
    employee1.isSenior = false;
    REQUIRE(employee1.Save());

    Employee employee2;
    employee2.name = "Jane Doe";
    employee2.isSenior = true;
    REQUIRE(employee2.Save());

    Employee employee3;
    employee3.name = "John Smith";
    employee3.isSenior = true;
    REQUIRE(employee3.Save());

    auto employees = Employee::Where("is_senior"sv, true).value();
    for (const auto& employee: employees)
        INFO("Employee: {}" << employee.Inspect()); // FIXME: breaks due to field name being NULL
    REQUIRE(employees.size() == 2);
    CHECK(employees[0].Id() == employee2.Id());
    CHECK(employees[0].name == employee2.name);
    CHECK(employees[1].Id() == employee3.Id());
    CHECK(employees[1].name == employee3.name);
}
