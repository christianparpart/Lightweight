# JPSql Model API

The JPSql Model API is a lightweight ORM that allows you to interact with your database using a simple and intuitive API.
It is designed to be easy to use while being as efficient as possible.

This API was inspired by **Active Record** pattern and API from Ruby on Rails.

## Features

- **Simple & Intuitive API**: The API is designed to be as simple as possible.
- **Efficient**: The API is designed to be as efficient as possible.

## Example

```cpp
#include <JPSql/Model.hpp>
#include <print>

struct Book;

struct Author: Model::Record<User>
{
    Model::Field<std::string, 2, "name"> name;
    Model::HasMany<Book> books;
};

struct Book: Model::Record<Book>
{
    Model::Field<std::string, 2, "title"> title;
    Model::Field<std::string, 3, "isbn"> isbn;
    Model::BelongsTo<Author, 4, "author_id"> author;
};

void demo()
{
    Model::CreateTables<Author, Book>();

    Author author;
    author.name = "Bjarne Stroustrup";
    author.Save().or_else(std::abort);

    Book book;
    book.title = "The C++ Programming Language";
    book.isbn = "978-0-321-56384-2";
    book.author = author;
    book.Save().or_else(std::abort);

    auto books = Book::All().or_else(std::abort);
    for (auto book: books)
        std::println("{}", book);

    std::println("{} has {} books", author.name, author.books.Count());

    author.Destroy();
    book.Destroy();
}
```

## TODO: Open Refactors

- [ ] Reintroduce `Model::Record<T, TableName, PrimaryKeyType, PrimaryKeyName>`
- [ ] Support (join) Associations (https://guides.rubyonrails.org/association_basics.html) (e.g. `Author::All().Join<Book>().Where<Book::isbn == "978-0-321-56384-2"`) (may need some higher level query builder)
- [ ] Consider changing `SqlVariant` into a wrapped `std::variant` to allow convenient conversions between types (e.g. `SqlVariant v = 42; v.ToInt() == 42; v.As<int>() == 42; v.As<std::string>() == "42";`)

## TODO: Associations

See: https://guides.rubyonrails.org/association_basics.html

- [x] `BelongsTo<T>` (basic foreign key support)
- [ ] `BelongsTo<T>` (support self-joinings)
- [ ] `HasOne<T>`
- [ ] `HasMany<T>`
- [ ] `HasOneThrough<T>`
- [ ] `HasManyThrough<T>`

## Open TODOs

- [ ] [JPSql] Add custom type `SqlText` for `TEXT` fields
- [ ] [JPSql] Add custom type `SqlFixedString` for `CHAR(N)` fields
- [ ] Add custom type `LegacyPrimaryKey` which does not make use of `AUTOINCREMENT` but auto-increments by first selecting the max value and incrementing it by one
- [ ] Add test case for `LegacyPrimaryKey` (also to demonstrate how to use it)
- [ ] Add std::formatter for `Record<T>`
- [ ] Add test for `BelongsTo<>` (including eager & lazy-loading check)
- [ ] Add test for `HasOne<>` (including eager & lazy-loading check)
- [ ] Add test for `HasMany<>` (including eager & lazy-loading check)
- [ ] Differenciate between VARCHAR (std::string) and TEXT (maybe SqlText<std::string>?)
- [x] Make logging more useful, adding payload data
- [x] remove debug prints
- [x] add proper trace logging