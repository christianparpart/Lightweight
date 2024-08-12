#include "../JPSql/Model/Logger.hpp"
#include "../JPSql/Model/Record.hpp"
#include "../JPSql/Model/Relation.hpp"
#include "../JPSql/Model/Utils.hpp"
#include "../JPSql/SqlConnection.hpp"
#include "JPSqlTestUtils.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

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
    Model::QueryLogger::Set(Model::QueryLogger::StandardLogger());

    return Catch::Session().run(argc, argv);
}

struct Author;
struct Book;

struct Book: Model::Record<Book>
{
    Model::Field<std::string, 2, "title"> title;
    Model::Field<std::string, 3, "isbn"> isbn;
    Model::BelongsTo<Author, 4, "author_id"> author;

    Book():
        Record { "books" },
        title { *this },
        isbn { *this },
        author { *this }
    {
    }

    Book(Book&& other) noexcept:
        Record { std::move(other) },
        title { *this, std::move(other.title) },
        isbn { *this, std::move(other.isbn) },
        author { *this, std::move(other.author) }
    {
    }
};

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

    Author(Author&& other) noexcept:
        Record { std::move(other) },
        name { *this, std::move(other.name) },
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

