// SPDX-License-Identifier: Apache-2.0

#include "Utils.hpp"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <functional>
#include <set>
#include <source_location>

struct QueryExpectations
{
    std::string_view sqlite;
    std::string_view sqlServer;

    static QueryExpectations All(std::string_view query)
    {
        return { query, query };
    }
};

template <typename TheSqlQuery>
    requires(std::is_invocable_v<TheSqlQuery, SqlQueryBuilder&>)
void checkSqlQueryBuilder(TheSqlQuery const& sqlQueryBuilder,
                          QueryExpectations const& expectations,
                          std::function<void()> const& postCheck = {},
                          std::source_location const& location = std::source_location::current())
{
    auto const eraseLinefeeds = [](std::string str) noexcept -> std::string {
        // Remove all LFs from str:
        str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
        return str;
    };
    INFO(std::format("Test source location: {}:{}", location.file_name(), location.line()));

    auto const& sqliteFormatter = SqlQueryFormatter::Sqlite();
    auto sqliteQueryBuilder = SqlQueryBuilder(sqliteFormatter);
    auto const actualSqlite = eraseLinefeeds(sqlQueryBuilder(sqliteQueryBuilder).ToSql());
    CHECK(actualSqlite == expectations.sqlite);
    if (postCheck)
        postCheck();

    auto const& sqlServerFormatter = SqlQueryFormatter::SqlServer();
    auto sqlServerQueryBuilder = SqlQueryBuilder(sqlServerFormatter);
    auto const actualSqlServer = eraseLinefeeds(sqlQueryBuilder(sqlServerQueryBuilder).ToSql());
    CHECK(actualSqlServer == expectations.sqlServer);
    if (postCheck)
        postCheck();
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Select.Count", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder([](SqlQueryBuilder& q) { return q.FromTable("Table").Select().Count(); },
                         QueryExpectations::All("SELECT COUNT(*) FROM \"Table\""));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Select.All", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTable("That").Select().Fields("a", "b").Field("c").GroupBy("a").OrderBy("b").All();
        },
        QueryExpectations::All(R"(SELECT "a", "b", "c" FROM "That" GROUP BY "a" ORDER BY "b" ASC)"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Select.Distinct.All", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTable("That").Select().Distinct().Fields("a", "b").Field("c").GroupBy("a").OrderBy("b").All();
        },
        QueryExpectations::All(R"(SELECT DISTINCT "a", "b", "c" FROM "That" GROUP BY "a" ORDER BY "b" ASC)"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Select.First", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) { return q.FromTable("That").Select().Field("field1").OrderBy("id").First(); },
        QueryExpectations {
            .sqlite = R"(SELECT "field1" FROM "That" ORDER BY "id" ASC LIMIT 1)",
            .sqlServer = R"(SELECT TOP 1 "field1" FROM "That" ORDER BY "id" ASC)",
        });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Select.Range", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTable("That").Select().Fields("foo", "bar").OrderBy("id").Range(200, 50);
        },
        QueryExpectations {
            .sqlite = R"(SELECT "foo", "bar" FROM "That" ORDER BY "id" ASC LIMIT 50 OFFSET 200)",
            .sqlServer = R"(SELECT "foo", "bar" FROM "That" ORDER BY "id" ASC OFFSET 200 ROWS FETCH NEXT 50 ROWS ONLY)",
        });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Delete", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) { return q.FromTable("That").Delete().Where("foo", 42).Where("bar", "baz"); },
        QueryExpectations::All(R"(DELETE FROM "That" WHERE "foo" = 42 AND "bar" = 'baz')"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Where.Junctors", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            // clang-format off
            return q.FromTable("Table")
                .Select()
                .WhereRaw("a")
                .And().WhereRaw("b")
                .Or().WhereRaw("c")
                .And().WhereRaw("d")
                .And().Not().WhereRaw("e")
                .Count();
            // clang-format on
        },
        QueryExpectations::All(R"SQL(SELECT COUNT(*) FROM "Table" WHERE a AND b OR c AND d AND NOT e)SQL"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.WhereIn", "[SqlQueryBuilder]")
{
    // Check functionality of container overloads for IN
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) { return q.FromTable("That").Delete().WhereIn("foo", std::vector { 1, 2, 3 }); },
        QueryExpectations::All(R"(DELETE FROM "That" WHERE "foo" IN (1, 2, 3))"));

    // Check functionality of an lvalue input range
    auto const values = std::set { 1, 2, 3 };
    checkSqlQueryBuilder([&](SqlQueryBuilder& q) { return q.FromTable("That").Delete().WhereIn("foo", values); },
                         QueryExpectations::All(R"(DELETE FROM "That" WHERE "foo" IN (1, 2, 3))"));

    // Check functionality of the initializer_list overload for IN
    checkSqlQueryBuilder([](SqlQueryBuilder& q) { return q.FromTable("That").Delete().WhereIn("foo", { 1, 2, 3 }); },
                         QueryExpectations::All(R"(DELETE FROM "That" WHERE "foo" IN (1, 2, 3))"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Join", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTable("That").Select().Fields("foo", "bar").InnerJoin("Other", "id", "that_id").All();
        },
        QueryExpectations::All(
            R"(SELECT "foo", "bar" FROM "That" INNER JOIN "Other" ON "Other"."id" = "That"."that_id")"));

    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTable("That").Select().Fields("foo", "bar").LeftOuterJoin("Other", "id", "that_id").All();
        },
        QueryExpectations::All(
            R"(SELECT "foo", "bar" FROM "That" LEFT OUTER JOIN "Other" ON "Other"."id" = "That"."that_id")"));

    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            using namespace std::string_view_literals;
            return q.FromTable("Table_A")
                .Select()
                .Fields({ "foo"sv, "bar"sv }, "Table_A")
                .Fields({ "that_foo"sv, "that_id"sv }, "Table_B")
                .LeftOuterJoin("Table_B", "id", "that_id")
                .Where(SqlQualifiedTableColumnName { .tableName = "Table_A", .columnName = "foo" }, 42)
                .All();
        },
        QueryExpectations::All("SELECT \"Table_A\".\"foo\", \"Table_A\".\"bar\","
                               " \"Table_B\".\"that_foo\", \"Table_B\".\"that_id\""
                               " FROM \"Table_A\""
                               " LEFT OUTER JOIN \"Table_B\" ON \"Table_B\".\"id\" = \"Table_A\".\"that_id\""
                               " WHERE \"Table_A\".\"foo\" = 42"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.SelectAs", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) { return q.FromTable("That").Select().FieldAs("foo", "F").FieldAs("bar", "B").All(); },
        QueryExpectations::All(R"(SELECT "foo" AS "F", "bar" AS "B" FROM "That")"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.FromTableAs", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTableAs("Other", "O")
                .Select()
                .Field(SqlQualifiedTableColumnName { "O", "foo" })
                .Field(SqlQualifiedTableColumnName { "O", "bar" })
                .All();
        },
        QueryExpectations::All(R"(SELECT "O"."foo", "O"."bar" FROM "Other" AS "O")"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Insert", "[SqlQueryBuilder]")
{
    std::vector<SqlVariant> boundValues;
    checkSqlQueryBuilder(
        [&](SqlQueryBuilder& q) {
            return q.FromTableAs("Other", "O")
                .Insert(&boundValues)
                .Set("foo", 42)
                .Set("bar", "baz")
                .Set("baz", SqlNullValue);
        },
        QueryExpectations::All(R"(INSERT INTO "Other" ("foo", "bar", "baz") VALUES (?, ?, NULL))"),
        [&]() {
            CHECK(boundValues.size() == 2);
            CHECK(std::get<int>(boundValues[0].value) == 42);
            CHECK(std::get<std::string_view>(boundValues[1].value) == "baz");
            boundValues.clear();
        });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Update", "[SqlQueryBuilder]")
{
    std::vector<SqlVariant> boundValues;
    checkSqlQueryBuilder(
        [&](SqlQueryBuilder& q) {
            return q.FromTableAs("Other", "O").Update(&boundValues).Set("foo", 42).Set("bar", "baz").Where("id", 123);
        },
        QueryExpectations::All(R"(UPDATE "Other" AS "O" SET "foo" = ?, "bar" = ? WHERE "id" = ?)"),
        [&]() {
            CHECK(boundValues.size() == 3);
            CHECK(std::get<int>(boundValues[0].value) == 42);
            CHECK(std::get<std::string_view>(boundValues[1].value) == "baz");
            CHECK(std::get<int>(boundValues[2].value) == 123);
            boundValues.clear();
        });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Where.Lambda", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTable("That")
                .Select()
                .Field("foo")
                .Where("a", 1)
                .OrWhere([](auto& q) { return q.Where("b", 2).Where("c", 3); })
                .All();
        },
        QueryExpectations::All(R"(SELECT "foo" FROM "That" WHERE "a" = 1 OR ("b" = 2 AND "c" = 3))"));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.WhereColumn", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(
        [](SqlQueryBuilder& q) {
            return q.FromTable("That").Select().Field("foo").WhereColumn("left", "=", "right").All();
        },
        QueryExpectations::All(R"(SELECT "foo" FROM "That" WHERE "left" = "right")"));
}

TEST_CASE_METHOD(SqlTestFixture, "Use SqlQueryBuilder for SqlStatement.ExecuteDirct", "[SqlQueryBuilder]")
{
    auto stmt = SqlStatement {};

    bool constexpr quoted = true;
    CreateEmployeesTable(stmt, quoted);
    FillEmployeesTable(stmt, quoted);

    stmt.ExecuteDirect(stmt.Connection().Query("Employees").Select().Fields("FirstName", "LastName").All());

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Alice");
}

TEST_CASE_METHOD(SqlTestFixture, "Use SqlQueryBuilder for SqlStatement.Prepare", "[SqlQueryBuilder]")
{
    auto stmt = SqlStatement {};

    bool constexpr quoted = true;
    CreateEmployeesTable(stmt, quoted);
    FillEmployeesTable(stmt, quoted);

    std::vector<SqlVariant> inputBindings;

    auto const sqlQuery =
        stmt.Connection().Query("Employees").Update(&inputBindings).Set("Salary", 55'000).Where("Salary", 50'000);

    REQUIRE(inputBindings.size() == 2);
    CHECK(std::get<int>(inputBindings[0].value) == 55'000);
    CHECK(std::get<int>(inputBindings[1].value) == 50'000);

    stmt.Prepare(sqlQuery);
    stmt.ExecuteWithVariants(inputBindings);

    stmt.ExecuteDirect(R"(SELECT "FirstName", "LastName", "Salary" FROM "Employees" WHERE "Salary" = 55000)");
    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Alice");
    CHECK(stmt.GetColumn<std::string>(2) == "Smith");
    CHECK(stmt.GetColumn<int>(3) == 55'000);
}

TEST_CASE_METHOD(SqlTestFixture, "Use SqlQueryBuilder for SqlStatement.Prepare: iterative", "[SqlQueryBuilder]")
{
    auto stmt = SqlStatement {};

    bool constexpr quoted = true;
    CreateLargeTable(stmt, quoted);

    // Prepare INSERT query
    auto insertQuery = stmt.Connection().Query("LargeTable").Insert(nullptr /* no auto-fill */);
    for (char c = 'A'; c <= 'Z'; ++c)
    {
        auto const columnName = std::string(1, c);
        insertQuery.Set(columnName, SqlWildcard);
    }
    stmt.Prepare(insertQuery);

    // Execute the same query 10 times

    for (int i = 0; i < 10; ++i)
    {
        // Prepare data (fill all columns naively)
        std::vector<SqlVariant> inputBindings;
        for (char c = 'A'; c <= 'Z'; ++c)
            inputBindings.emplace_back(std::string(1, c) + std::to_string(i));

        // Execute the query with the prepared data
        stmt.ExecuteWithVariants(inputBindings);
    }
}

struct MFCLikeCString
{
    std::string value;

    [[nodiscard]] decltype(auto) GetString() const noexcept
    {
        return value.c_str();
    }

    [[nodiscard]] int GetLength() const noexcept
    {
        return static_cast<int>(value.size());
    }
};

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder: sub select with Where", "[SqlQueryBuilder]")
{
    auto sharedConnection = SqlConnection {};
    auto stmt = SqlStatement { sharedConnection };

    stmt.ExecuteDirect(R"SQL(DROP TABLE IF EXISTS "Test")SQL");
    stmt.ExecuteDirect(R"SQL(
        CREATE TABLE "Test" (
            "name" VARCHAR(20) NULL,
            "secret" INT NULL
        )
    )SQL");

    stmt.Prepare(R"SQL(INSERT INTO "Test" ("name", "secret") VALUES (?, ?))SQL");
    auto const names = std::vector<SqlFixedString<20>> { "Alice", "Bob", "Charlie", "David" };
    auto const secrets = std::vector<int> { 42, 43, 44, 45 };
    stmt.ExecuteBatchSoft(names, secrets);

    auto const totalRecords = stmt.ExecuteDirectSingle<int>(R"SQL(SELECT COUNT(*) FROM "Test")SQL");
    REQUIRE(totalRecords.value_or(0) == 4);

    // clang-format off
    auto const subSelect = stmt.Query("Test")
                              .Select()
                              .Field("secret")
                              .Where("name", "Alice")
                              .All();
    auto const selectQuery = stmt.Query("Test")
                                 .Select()
                                 .Fields({ "name", "secret" })
                                 .Where("secret", subSelect)
                                 .All();
    // clang-format on
    stmt.Prepare(selectQuery);
    stmt.Execute();

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Alice");
    CHECK(stmt.GetColumn<int>(2) == 42);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder: sub select with WhereIn", "[SqlQueryBuilder]")
{
    auto stmt = SqlStatement {};

    stmt.ExecuteDirect(R"SQL(DROP TABLE IF EXISTS "Test")SQL");
    stmt.ExecuteDirect(R"SQL(
        CREATE TABLE "Test" (
            "name" VARCHAR(20) NULL,
            "secret" INT NULL
        )
    )SQL");

    stmt.Prepare(R"SQL(INSERT INTO "Test" ("name", "secret") VALUES (?, ?))SQL");
    auto const names = std::vector<SqlFixedString<20>> { "Alice", "Bob", "Charlie", "David" };
    auto const secrets = std::vector<int> { 42, 43, 44, 45 };
    stmt.ExecuteBatchSoft(names, secrets);

    auto const totalRecords = stmt.ExecuteDirectSingle<int>("SELECT COUNT(*) FROM \"Test\"");
    REQUIRE(totalRecords.value_or(0) == 4);

    // clang-format off
    auto const subSelect = stmt.Query("Test")
                              .Select()
                              .Field("secret")
                              .Where("name", "Alice")
                              .OrWhere("name", "Bob").All();
    auto const selectQuery = stmt.Query("Test")
                                 .Select()
                                 .Fields({ "name", "secret" })
                                 .WhereIn("secret", subSelect)
                                 .All();
    // clang-format on
    stmt.Prepare(selectQuery);
    stmt.Execute();

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Alice");
    CHECK(stmt.GetColumn<int>(2) == 42);

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Bob");
    CHECK(stmt.GetColumn<int>(2) == 43);

    REQUIRE(!stmt.FetchRow());
}
