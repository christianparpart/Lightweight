#include "../JPSql/Model/All.hpp"
#include "../JPSql/SqlConnection.hpp"
#include "JPSqlTestUtils.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace std::string_view_literals;

struct Person;
struct Company;
struct Phone;
struct Job;

int main(int argc, char** argv)
{
    auto result = SqlTestFixture::Initialize(argc, argv);
    if (!result.has_value())
        return result.error();

    std::tie(argc, argv) = result.value();
    return Catch::Session().run(argc, argv);
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

    CreateModelTable<MovableRecord>();

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

    CreateModelTable<TrimmedStringRecord>();

    TrimmedStringRecord record;
    record.name = SqlTrimmedString { "  Hello, World!  " };
    record.Save();
    record.Reload(); // Ensure we fetch name from the database and got trimmed on fetch.

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
    Model::Field<SqlTrimmedFixedString<64>, 2, "title"> title;
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
    CreateModelTable<Author>();
    CreateModelTable<Book>();

    Author author;
    author.name = "Bjarne Stroustrup";
    author.Save();
    REQUIRE(author.Id() == 1);
    REQUIRE(author.books.Count() == 0);

    Book book1;
    book1.title = "The C++ Programming Language";
    book1.isbn = "978-0-321-56384-2";
    book1.author = author;
    book1.Save();
    REQUIRE(book1.Id() == 1);
    REQUIRE(Book::Count() == 1);
    REQUIRE(author.books.Count() == 1);

    Book book2;
    book2.title = "A Tour of C++";
    book2.isbn = "978-0-321-958310";
    book2.author = author;
    book2.Save();
    REQUIRE(book2.Id() == 2);
    REQUIRE(Book::Count() == 2);
    REQUIRE(author.books.Count() == 2);

    // Also take the chance to ensure the formatter works.
    REQUIRE(std::format("{}", author) == author.Inspect());
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Load", "[model]")
{
    Model::CreateSqlTables<Author, Book>();

    Author author;
    author.name = "Bjarne Stroustrup";
    author.Save();

    Book book;
    book.title = "The C++ Programming Language";
    book.isbn = "978-0-321-56384-2";
    book.author = author;
    book.Save();

    Book bookLoaded;
    bookLoaded.Load(book.Id());
    INFO("Book: " << book);
    CHECK(bookLoaded.Id() == book.Id());
    CHECK(bookLoaded.title == book.title);
    CHECK(bookLoaded.isbn == book.isbn);
    CHECK(bookLoaded.author == book.author);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Find", "[model]")
{
    Model::CreateSqlTables<Author, Book>();

    Author author;
    author.name = "Bjarne Stroustrup";
    author.Save();

    Book book;
    book.title = "The C++ Programming Language";
    book.isbn = "978-0-321-56384-2";
    book.author = author;
    book.Save();

    Book bookLoaded = Book::Find(book.Id()).value();
    INFO("Book: " << book);
    CHECK(bookLoaded.Id() == book.Id());     // primary key
    CHECK(bookLoaded.title == book.title);   // Field<>
    CHECK(bookLoaded.isbn == book.isbn);     // Field<>
    CHECK(bookLoaded.author == book.author); // BelongsTo<>
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Update", "[model]")
{
    Model::CreateSqlTables<Author, Book>();

    Author author;
    author.name = "Bjarne Stroustrup";
    author.Save();

    Book book;
    book.title = "The C++ Programming Language";
    book.isbn = "978-0-321-56384-2";
    book.author = author;
    book.Save();

    book.isbn = "978-0-321-958310";
    book.Save();

    Book bookRead = Book::Find(book.Id()).value();
    CHECK(bookRead.Id() == book.Id());
    CHECK(bookRead.title == book.title);
    CHECK(bookRead.isbn == book.isbn);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.Destroy", "[model]")
{
    CreateModelTable<Author>();

    Author author1;
    author1.name = "Bjarne Stroustrup";
    author1.Save();
    REQUIRE(Author::Count() == 1);

    Author author2;
    author2.name = "John Doe";
    author2.Save();
    REQUIRE(Author::Count() == 2);

    author1.Destroy();
    REQUIRE(Author::Count() == 1);
}

TEST_CASE_METHOD(SqlTestFixture, "Model.All", "[model]")
{
    CreateModelTable<Author>();

    Author author1;
    author1.name = "Bjarne Stroustrup";
    author1.Save();

    Author author2;
    author2.name = "John Doe";
    author2.Save();

    Author author3;
    author3.name = "Some very long name";
    author3.Save();

    Author author4;
    author4.name = "Shorty";
    author4.Save();

    auto authors = Author::All();
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
    CreateModelTable<ColumnTypesRecord>();

    ColumnTypesRecord record;
    record.stringColumn = "Hello";
    record.textColumn = SqlText { ", World!" };
    record.Save();

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
    CreateModelTable<Employee>();

    Employee employee1;
    employee1.name = "John Doe";
    employee1.isSenior = false;
    employee1.Save();

    Employee employee2;
    employee2.name = "Jane Doe";
    employee2.isSenior = true;
    employee2.Save();

    Employee employee3;
    employee3.name = "John Smith";
    employee3.isSenior = true;
    employee3.Save();

    auto employees = Employee::Where("is_senior"sv, true).All();
    for (const auto& employee: employees)
        INFO("Employee: {}" << employee); // FIXME: breaks due to field name being NULL
    REQUIRE(employees.size() == 2);
    CHECK(employees[0].Id() == employee2.Id());
    CHECK(employees[0].name == employee2.name);
    CHECK(employees[1].Id() == employee3.Id());
    CHECK(employees[1].name == employee3.name);
}
