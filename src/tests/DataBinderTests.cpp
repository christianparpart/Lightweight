// SPDX-License-Identifier: Apache-2.0

#include "Utils.hpp"

#include <Lightweight/DataBinder/UnicodeConverter.hpp>
#include <Lightweight/SqlConnection.hpp>
#include <Lightweight/SqlDataBinder.hpp>
#include <Lightweight/SqlQuery.hpp>
#include <Lightweight/SqlQueryFormatter.hpp>
#include <Lightweight/SqlScopedTraceLogger.hpp>
#include <Lightweight/SqlStatement.hpp>
#include <Lightweight/SqlTransaction.hpp>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstdlib>
#include <format>
#include <ranges>
#include <type_traits>

// NOLINTBEGIN(readability-container-size-empty)

#if defined(_MSC_VER)
    // Disable the warning C4834: discarding return value of function with 'nodiscard' attribute.
    // Because we are simply testing and demonstrating the library and not using it in production code.
    #pragma warning(disable : 4834)
#endif

using namespace std::string_view_literals;

struct CustomType
{
    int value;
};

template <>
struct SqlDataBinder<CustomType>
{
    static SQLRETURN InputParameter(SQLHSTMT hStmt,
                                    SQLUSMALLINT column,
                                    CustomType const& value,
                                    SqlDataBinderCallback& cb) noexcept
    {
        return SqlDataBinder<int>::InputParameter(hStmt, column, value.value, cb);
    }

    static SQLRETURN OutputColumn(SQLHSTMT hStmt,
                                  SQLUSMALLINT column,
                                  CustomType* result,
                                  SQLLEN* indicator,
                                  SqlDataBinderCallback& callback) noexcept
    {
        callback.PlanPostProcessOutputColumn([result]() { result->value = PostProcess(result->value); });
        return SqlDataBinder<int>::OutputColumn(hStmt, column, &result->value, indicator, callback);
    }

    static SQLRETURN GetColumn(SQLHSTMT hStmt,
                               SQLUSMALLINT column,
                               CustomType* result,
                               SQLLEN* indicator,
                               SqlDataBinderCallback const& cb) noexcept
    {
        return SqlDataBinder<int>::GetColumn(hStmt, column, &result->value, indicator, cb);
    }

    static constexpr int PostProcess(int value) noexcept
    {
        return value | 0x01;
    }
};

TEST_CASE_METHOD(SqlTestFixture, "custom types", "[SqlDataBinder]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value INT NULL)");

    // check custom type handling for input parameters
    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(CustomType { 42 });

    // check custom type handling for explicitly fetched output columns
    auto result = stmt.ExecuteDirectSingle<CustomType>("SELECT Value FROM Test");
    REQUIRE(result.value().value == 42);

    // check custom type handling for bound output columns
    result = {};
    stmt.Prepare("SELECT Value FROM Test");
    stmt.Execute();
    {
        auto reader = stmt.GetResultCursor();
        reader.BindOutputColumns(&result);
        REQUIRE(reader.FetchRow());
        REQUIRE(result.value().value == (42 | 0x01));
    }

    // Test inserting a NULL value
    stmt.ExecuteDirect("DELETE FROM Test");
    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(SqlNullValue);
    auto const y = stmt.ExecuteDirectSingle<CustomType>("SELECT Value FROM Test");
    CHECK(!y);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: resize and clear", "[SqlFixedString]")
{
    SqlFixedString<8> str;

    REQUIRE(str.size() == 0);
    REQUIRE(str.empty());

    str.resize(1, 'x');
    REQUIRE(!str.empty());
    REQUIRE(str.size() == 1);
    REQUIRE(str == "x");

    str.resize(4, 'y');
    REQUIRE(str.size() == 4);
    REQUIRE(str == "xyyy");

    // one-off overflow truncates
    str.resize(9, 'z');
    REQUIRE(str.size() == 8);
    REQUIRE(str == "xyyyzzzz");

    // resize down
    str.resize(2);
    REQUIRE(str.size() == 2);
    REQUIRE(str == "xy");

    str.clear();
    REQUIRE(str.size() == 0);
    REQUIRE(str == "");
}

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: push_back and pop_back", "[SqlFixedString]")
{
    SqlFixedString<2> str;

    str.push_back('a');
    str.push_back('b');
    REQUIRE(str == "ab");

    // overflow: no-op (truncates)
    str.push_back('c');
    REQUIRE(str == "ab");

    str.pop_back();
    REQUIRE(str == "a");

    str.pop_back();
    REQUIRE(str == "");

    // no-op
    str.pop_back();
    REQUIRE(str == "");
}

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: assign", "[SqlFixedString]")
{
    SqlFixedString<12> str;
    str.assign("Hello, World");
    REQUIRE(str == "Hello, World");
    // str.assign("Hello, World!"); <-- would fail due to static_assert
    str.assign("Hello, World!"sv);
    REQUIRE(str == "Hello, World");

    str = "Something";
    REQUIRE(str == "Something");
    // str = ("Hello, World!"); // <-- would fail due to static_assert
}

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: c_str", "[SqlFixedString]")
{
    SqlFixedString<12> str { "Hello, World" };
    str.resize(5);
    REQUIRE(str.data()[5] == ',');

    SqlFixedString<12> const& constStr = str;
    REQUIRE(constStr.c_str() == "Hello"sv); // Call to `c_str() const` also mutates [5] to NUL
    REQUIRE(str.data()[5] == '\0');

    str.resize(2);
    REQUIRE(str.data()[2] == 'l');
    REQUIRE(str.c_str() == "He"sv); // Call to `c_str()` also mutates [2] to NUL
    REQUIRE(str.data()[2] == '\0');
}

TEST_CASE_METHOD(SqlTestFixture,
                 "SqlTrimmedString: FetchRow can auto-trim string if requested",
                 "[SqlDataBinder],[SqlTrimmedString]")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
    stmt.Execute("Alice    ", SqlNullValue, 50'000);

    SqlTrimmedString firstName { .value = std::string(20, '\0') };
    std::optional<SqlTrimmedString> lastName { SqlTrimmedString { .value = std::string(20, '\0') } };

    stmt.ExecuteDirect("SELECT FirstName, LastName FROM Employees");
    stmt.BindOutputColumns(&firstName, &lastName);

    REQUIRE(stmt.FetchRow());
    CHECK(firstName.value == "Alice");
    CHECK(!lastName.has_value());

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: GetColumn in-place store variant", "[SqlDataBinder]")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);

    stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
    stmt.Execute("Alice", SqlNullValue, 50'000);

    stmt.ExecuteDirect("SELECT FirstName, LastName, Salary FROM Employees");
    REQUIRE(stmt.FetchRow());

    CHECK(stmt.GetColumn<std::string>(1) == "Alice");

    SqlVariant lastName;
    CHECK(!stmt.GetColumn(2, &lastName));
    CHECK(lastName.IsNull());

    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    SqlVariant salary;
    CHECK(stmt.GetColumn(3, &salary));
    CHECK(salary.TryGetInt().value_or(0) == 50'000);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: NULL values", "[SqlDataBinder],[SqlVariant]")
{
    auto stmt = SqlStatement();
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks VARCHAR(50) NULL)");

    stmt.Prepare("INSERT INTO Test (Remarks) VALUES (?)");
    stmt.Execute(SqlNullValue);

    stmt.ExecuteDirect("SELECT Remarks FROM Test");
    {
        auto reader = stmt.GetResultCursor();
        REQUIRE(reader.FetchRow());

        auto const actual = reader.GetColumn<SqlVariant>(1);
        CHECK(std::holds_alternative<SqlNullType>(actual.value));
    }

    // Test for inserting/getting NULL values
    stmt.ExecuteDirect("DELETE FROM Test");
    stmt.Prepare("INSERT INTO Test (Remarks) VALUES (?)");
    stmt.Execute(SqlNullValue);
    auto const result = stmt.ExecuteDirectSingle<SqlVariant>("SELECT Remarks FROM Test");
    CHECK(result.IsNull());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlDate", "[SqlDataBinder],[SqlVariant]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value DATE NULL)");

    using namespace std::chrono_literals;
    auto const expected = SqlVariant { SqlDate { 2017y, std::chrono::August, 16d } };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expected);

    stmt.ExecuteDirect("SELECT Value FROM Test");
    {
        auto reader = stmt.GetResultCursor();
        REQUIRE(reader.FetchRow());
        auto const actual = reader.GetColumn<SqlVariant>(1);
        CHECK(std::get<SqlDate>(actual.value) == std::get<SqlDate>(expected.value));
    }

    // Test for inserting/getting NULL values
    stmt.ExecuteDirect("DELETE FROM Test");
    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(SqlNullValue);
    auto const result = stmt.ExecuteDirectSingle<SqlVariant>("SELECT Value FROM Test");
    CHECK(result.IsNull());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlTime", "[SqlDataBinder],[SqlVariant]")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect("CREATE TABLE Test (Value TIME NULL)");

    using namespace std::chrono_literals;
    auto const expected = SqlVariant { SqlTime { 12h, 34min, 56s } };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expected);

    auto const actual = stmt.ExecuteDirectSingle<SqlVariant>("SELECT Value FROM Test");

    if (stmt.Connection().ServerType() == SqlServerType::POSTGRESQL)
    {
        WARN("PostgreSQL seems to report SQL_TYPE_DATE here. Skipping check, that would fail otherwise.");
        // TODO: Find out why PostgreSQL reports SQL_TYPE_DATE instead of SQL_TYPE_TIME for SQL column type TIME.
        return;
    }

    CHECK(std::get<SqlTime>(actual.value) == std::get<SqlTime>(expected.value));

    // Test for inserting/getting NULL values
    stmt.ExecuteDirect("DELETE FROM Test");
    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(SqlNullValue);
    auto const result = stmt.ExecuteDirectSingle<SqlVariant>("SELECT Value FROM Test");
    CHECK(result.IsNull());
}

TEST_CASE_METHOD(SqlTestFixture, "std::optional: InputParameter", "[SqlDataBinder],[std::optional]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks1 VARCHAR(50) NULL, Remarks2 VARCHAR(50) NULL)");
    stmt.Prepare("INSERT INTO Test (Remarks1, Remarks2) VALUES (?, ?)");
    stmt.Execute("Blurb", std::optional<std::string> {});

    stmt.ExecuteDirect("SELECT Remarks1, Remarks2 FROM Test");
    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Blurb");
    CHECK(!stmt.GetColumn<std::optional<std::string>>(2).has_value());
}

TEST_CASE_METHOD(SqlTestFixture, "std::optional: BindOutputColumns", "[SqlDataBinder],[std::optional]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks1 VARCHAR(50) NULL, Remarks2 VARCHAR(50) NULL)");
    stmt.Prepare("INSERT INTO Test (Remarks1, Remarks2) VALUES (?, ?)");
    stmt.Execute("Blurb", SqlNullValue);

    stmt.ExecuteDirect("SELECT Remarks1, Remarks2 FROM Test");

    auto actual1 = std::optional<std::string> {};
    auto actual2 = std::optional<std::string> {};
    stmt.BindOutputColumns(&actual1, &actual2);
    REQUIRE(stmt.FetchRow());
    CHECK(actual1.value_or("IS_NULL") == "Blurb");
    CHECK(!actual2.has_value());
}

TEST_CASE_METHOD(SqlTestFixture, "std::optional: GetColumn", "[SqlDataBinder],[std::optional]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks1 VARCHAR(50) NULL, Remarks2 VARCHAR(50) NULL)");
    stmt.Prepare("INSERT INTO Test (Remarks1, Remarks2) VALUES (?, ?)");
    stmt.Execute("Blurb", SqlNullValue);

    stmt.ExecuteDirect("SELECT Remarks1, Remarks2 FROM Test");
    REQUIRE(stmt.FetchRow());
    auto const actual1 = stmt.GetColumn<std::optional<std::string>>(1);
    auto const actual2 = stmt.GetColumn<std::optional<std::string>>(2);
    CHECK(actual1.value_or("IS_NULL") == "Blurb");
    CHECK(!actual2.has_value());
}

TEST_CASE_METHOD(SqlTestFixture, "InputParameter and GetColumn for very large values", "[SqlDataBinder]")
{
    auto const MakeLargeText = [](size_t size) {
        auto text = std::string(size, '\0');
        std::ranges::generate(text, [i = 0]() mutable { return char('A' + (i++ % 26)); });
        return text;
    };

    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect("CREATE TABLE Test (Value TEXT)");
    auto const expectedText = MakeLargeText(8 * 1000);
    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expectedText);

    SECTION("check handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        CHECK(stmt.GetColumn<std::string>(1) == expectedText);
    }

    SECTION("check handling for explicitly fetched output columns (in-place store)")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        std::string actualText;
        CHECK(stmt.GetColumn(1, &actualText));
        CHECK(actualText == expectedText);
    }

    SECTION("check handling for bound output columns")
    {
        stmt.Prepare("SELECT Value FROM Test");
        stmt.Execute();
        {
            auto reader = stmt.GetResultCursor();
            std::string actualText; // intentionally an empty string, auto-growing behind the scenes
            reader.BindOutputColumns(&actualText);
            REQUIRE(reader.FetchRow());
            REQUIRE(actualText.size() == expectedText.size());
            CHECK(actualText == expectedText);
        }
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlFixedString", "[SqlDataBinder],[SqlFixedString]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value VARCHAR(8) NULL)");

    auto const expectedValue = SqlFixedString<8> { "Hello " };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expectedValue);

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        auto const actualValue = stmt.ExecuteDirectSingle<SqlFixedString<8>>("SELECT Value FROM Test");
        CHECK(actualValue.value() == expectedValue);

        SECTION("Truncated result")
        {
            auto const truncatedValueOpt = stmt.ExecuteDirectSingle<SqlFixedString<4>>("SELECT Value FROM Test");
            auto const truncatedValue = truncatedValueOpt.value();
            auto const truncatedStrView = truncatedValue.substr(0);
            auto const expectedStrView = expectedValue.substr(0, 3);
            CHECK(truncatedStrView == expectedStrView); // "Hel"
        }

        SECTION("Trimmed result")
        {
            auto const trimmedValue = stmt.ExecuteDirectSingle<SqlTrimmedFixedString<8>>("SELECT Value FROM Test");
            CHECK(trimmedValue.value() == "Hello");
        }
    }

    SECTION("check custom type handling for bound output columns")
    {
        stmt.Prepare("SELECT Value FROM Test");
        auto actualValue = SqlFixedString<8> {};
        stmt.BindOutputColumns(&actualValue);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == expectedValue);
    }

    SECTION("check custom type handling for bound output columns (trimmed)")
    {
        stmt.Prepare("SELECT Value FROM Test");
        auto actualValue = SqlTrimmedFixedString<8> {};
        stmt.BindOutputColumns(&actualValue);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == "Hello");
    }

    SECTION("check for NULL values")
    {
        stmt.ExecuteDirect("DELETE FROM Test");
        stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
        stmt.Execute(SqlNullValue);
        auto const result = stmt.ExecuteDirectSingle<SqlFixedString<8>>("SELECT Value FROM Test");
        CHECK(!result.has_value());
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlText", "[SqlDataBinder],[SqlText]")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect("CREATE TABLE Test (Value TEXT NULL)");

    using namespace std::chrono_literals;
    auto const expectedValue = SqlText { "Hello, World!" };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expectedValue);

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actualValue = stmt.GetColumn<SqlText>(1);
        CHECK(actualValue == expectedValue);
    }

    SECTION("check custom type handling for bound output columns")
    {
        stmt.Prepare("SELECT Value FROM Test");
        auto actualValue = SqlText {};
        stmt.BindOutputColumns(&actualValue);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == expectedValue);
    }

    SECTION("check for NULL values")
    {
        stmt.ExecuteDirect("DELETE FROM Test");
        stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
        stmt.Execute(SqlNullValue);
        auto const result = stmt.ExecuteDirectSingle<SqlText>("SELECT Value FROM Test");
        CHECK(!result.has_value());
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlDateTime", "[SqlDataBinder],[SqlDateTime]")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect(std::format("CREATE TABLE Test (Value {} NULL)",
                                   stmt.Connection().Traits().ColumnTypeName(SqlColumnType::DATETIME)));

    // With SQL Server or Oracle, we could use DATETIME2(7) and have nano-second precision (with 100ns resolution)
    // The standard DATETIME and ODBC SQL_TIMESTAMP have only millisecond precision.

    using namespace std::chrono_literals;
    auto const expectedValue = SqlDateTime(2017y, std::chrono::August, 16d, 17h, 30min, 45s, 123'000'000ns);

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expectedValue);

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actualValue = stmt.GetColumn<SqlDateTime>(1);
        CHECK(actualValue == expectedValue);
    }

    SECTION("check custom type handling for bound output columns")
    {
        stmt.Prepare("SELECT Value FROM Test");
        auto actualValue = SqlDateTime {};
        stmt.BindOutputColumns(&actualValue);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == expectedValue);
    }

    SECTION("check for NULL values")
    {
        stmt.ExecuteDirect("DELETE FROM Test");
        stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
        stmt.Execute(SqlNullValue);
        auto const result = stmt.ExecuteDirectSingle<SqlDateTime>("SELECT Value FROM Test");
        CHECK(!result.has_value());
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlDate", "[SqlDataBinder],[SqlDate]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value DATE NULL)");
    using namespace std::chrono_literals;
    auto const expected = SqlDate { std::chrono::year_month_day { 2017y, std::chrono::August, 16d } };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expected);

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlDate>(1);
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlDate>(1);
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for bound output columns")
    {
        stmt.Prepare("SELECT Value FROM Test");
        auto actual = SqlDate {};
        stmt.BindOutputColumns(&actual);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        REQUIRE(actual == expected);
    }

    SECTION("check for NULL values")
    {
        stmt.ExecuteDirect("DELETE FROM Test");
        stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
        stmt.Execute(SqlNullValue);
        auto const result = stmt.ExecuteDirectSingle<SqlDate>("SELECT Value FROM Test");
        CHECK(!result.has_value());
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlTime", "[SqlDataBinder],[SqlTime]")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect("CREATE TABLE Test (Value TIME NULL)");
    using namespace std::chrono_literals;
    auto const expected = SqlTime(12h, 34min, 56s);

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expected);

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlTime>(1);
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlTime>(1);
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for bound output columns")
    {
        stmt.Prepare("SELECT Value FROM Test");
        auto actual = SqlTime {};
        stmt.BindOutputColumns(&actual);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        REQUIRE(actual == expected);
    }

    SECTION("check for NULL values")
    {
        stmt.ExecuteDirect("DELETE FROM Test");
        stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
        stmt.Execute(SqlNullValue);
        auto const result = stmt.ExecuteDirectSingle<SqlTime>("SELECT Value FROM Test");
        CHECK(!result.has_value());
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder: Unicode", "[SqlDataBinder],[Unicode]")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);

    if (stmt.Connection().ServerType() == SqlServerType::SQLITE)
        // SQLite does UTF-8 by default, so we need to switch to UTF-16
        stmt.ExecuteDirect("PRAGMA encoding = 'UTF-16'");

    // Create table with Unicode column.
    // Mind, for PostgreSQL, we need to use VARCHAR instead of NVARCHAR,
    // because supports Unicode only via UTF-8.
    stmt.ExecuteDirect(
        std::format("CREATE TABLE Test (Value {}(50) NULL)",
                    stmt.Connection().ServerType() == SqlServerType::POSTGRESQL ? "VARCHAR" : "NVARCHAR"));

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");

    // Insert via wide string literal
    stmt.Execute(WTEXT("Wide string literal \U0001F600"));

    // Insert via wide string view
    stmt.Execute(WideStringView(WTEXT("Wide string literal \U0001F600")));

    // Insert via wide string object
    WideString const inputValue = WTEXT("Wide string literal \U0001F600");
    stmt.Execute(inputValue);

    stmt.ExecuteDirect("SELECT Value FROM Test");
    {
        auto reader = stmt.GetResultCursor();

        // Fetch and check GetColumn for wide string
        REQUIRE(reader.FetchRow());
        auto const actualValue = reader.GetColumn<WideString>(1);
        CHECK(actualValue == inputValue);

        // Bind output column, fetch, and check result in output column for wide string
        WideString actualValue2;
        reader.BindOutputColumns(&actualValue2);
        REQUIRE(reader.FetchRow());
        CHECK(actualValue2 == inputValue);
    }

    // Test for inserting/getting NULL VALUES
    stmt.ExecuteDirect("DELETE FROM Test");
    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(SqlNullValue);
    auto const result = stmt.ExecuteDirectSingle<WideString>("SELECT Value FROM Test");
    CHECK(!result.has_value());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder: SqlGuid", "[SqlDataBinder],[SqlGuid]")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);

    stmt.ExecuteDirect(std::format("CREATE TABLE Test (id {}, nullableGuid {} NULL, name VARCHAR(50) NULL)",
                                   stmt.Connection().Traits().PrimaryKeyGuidColumnType,
                                   stmt.Connection().Traits().GuidColumnType));

    auto const expectedGuid = SqlGuid::Create();
    auto const expectedGuidStr = std::format("{}", expectedGuid);

    stmt.Prepare("INSERT INTO Test (id, nullableGuid, name) VALUES (?, ?, ?)");
    stmt.Execute(expectedGuid, expectedGuid, "Alice");

    // Fetch and check GetColumn for GUID
    stmt.ExecuteDirect("SELECT id, nullableGuid, name FROM Test");
    {
        auto reader = stmt.GetResultCursor();
        REQUIRE(reader.FetchRow());
        auto const actualGuid = reader.GetColumn<SqlGuid>(1);
        auto const actualGuid2 = reader.GetColumn<SqlGuid>(2);
        auto const actualGuidStr = std::format("{}", actualGuid);
        CHECK(actualGuidStr == expectedGuidStr);
        CHECK(actualGuid == expectedGuid);
        CHECK(actualGuid2 == expectedGuid);
    }

    // Bind output column, fetch, and check result in output column for GUID
    stmt.ExecuteDirect("SELECT id FROM Test");
    {
        auto reader = stmt.GetResultCursor();
        SqlGuid actualGuid3;
        reader.BindOutputColumns(&actualGuid3);
        REQUIRE(reader.FetchRow());
        CHECK(actualGuid3 == expectedGuid);
        REQUIRE(!stmt.FetchRow());
    }

    // Test SELECT by GUID
    stmt.Prepare("SELECT name FROM Test WHERE id = ?");
    stmt.Execute(expectedGuid);
    {
        auto reader = stmt.GetResultCursor();
        REQUIRE(stmt.FetchRow());
        CHECK(stmt.GetColumn<std::string>(1) == "Alice");
        REQUIRE(!stmt.FetchRow());
    }

    // Test for inserting/getting NULL values
    stmt.ExecuteDirect("DELETE FROM Test");
    stmt.Prepare("INSERT INTO Test (nullableGuid, name) VALUES (?, ?)");
    stmt.Execute(SqlNullValue, "Alice");
    auto const result = stmt.ExecuteDirectSingle<SqlGuid>("SELECT nullableGuid FROM Test");
    CHECK(!result.has_value());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder: SqlNumeric", "[SqlDataBinder],[SqlNumeric]")
{
    auto stmt = SqlStatement {};

    UNSUPPORTED_DATABASE(stmt, SqlServerType::SQLITE); // Actually, SQLite3 does not support NUMERIC(p, s) type.
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);

    stmt.ExecuteDirect("DROP TABLE IF EXISTS Test");
    stmt.ExecuteDirect("CREATE TABLE Test (Value NUMERIC(10, 2) NULL)");

    auto const expectedValue = SqlNumeric<10, 2> { 123.45 };

    INFO(expectedValue);
    CHECK_THAT(expectedValue.ToDouble(), Catch::Matchers::WithinAbs(123.45, 0.001));
    CHECK_THAT(expectedValue.ToFloat(), Catch::Matchers::WithinAbs(123.45F, 0.001));
    CHECK(expectedValue.ToString() == "123.45");

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expectedValue);
    auto const actual = stmt.ExecuteDirectSingle<SqlNumeric<10, 2>>("SELECT Value FROM Test");
    REQUIRE(actual.has_value());
    CHECK(*actual == expectedValue);

    SECTION("Fetch and check GetColumn for the numeric")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actualValue = stmt.GetColumn<SqlNumeric<10, 2>>(1);
        CHECK(actualValue == expectedValue);
    }

    SECTION("Bind output column, fetch, and check result in output column for the numeric")
    {
        stmt.Prepare("SELECT Value FROM Test");
        SqlNumeric<10, 2> actualValue;
        stmt.BindOutputColumns(&actualValue);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == expectedValue);
    }

    SECTION("Test for inserting/getting NULL values")
    {
        stmt.ExecuteDirect("DELETE FROM Test");
        stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
        stmt.Execute(SqlNullValue);
        auto const result = stmt.ExecuteDirectSingle<SqlNumeric<10, 2>>("SELECT Value FROM Test");
        CHECK(!result.has_value());
    }
}

// NOLINTEND(readability-container-size-empty)
