#include "Utils.hpp"

#include <Lightweight/DataMapper/DataMapper.hpp>

#include <reflection-cpp/reflection.hpp>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <iostream>

template <typename>
struct TableTraits;

struct Person
{
    DataMapper::Field<int, 1, "id"> id;
    DataMapper::Field<SqlTrimmedFixedString<25>, 2, "name", DataMapper::SqlNullable> name;
    DataMapper::Field<bool, 3, "is_active"> is_active { true };

    static inline constexpr std::string_view TableName = "persons";
    static inline constexpr std::string_view PrimaryKey = "id";
};

TEST_CASE_METHOD(SqlTestFixture, "simple", "[DataMapper]")
{
    DataMapper::CreateTable<Person>();

    auto person = Person {};
    person.name = "John Doe";
    person.is_active = true;
    auto const s = Reflection::Inspect(person);
    std::cout << std::format("dump: '{}'\n", s);

    DataMapper::Save(person);
}
