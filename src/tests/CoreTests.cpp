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
#include <array>
#include <cstdlib>
#include <format>
#include <list>
#include <set>
#include <type_traits>

// NOLINTBEGIN(readability-container-size-empty)

#if defined(_MSC_VER)
    // Disable the warning C4834: discarding return value of function with 'nodiscard' attribute.
    // Because we are simply testing and demonstrating the library and not using it in production code.
    #pragma warning(disable : 4834)
#endif

using namespace std::string_view_literals;

#define UNSUPPORTED_DATABASE(stmt, dbType)                                                           \
    if ((stmt).Connection().ServerType() == (dbType))                                                \
    {                                                                                                \
        WARN(std::format("TODO({}): This database is currently unsupported on this test.", dbType)); \
        return;                                                                                      \
    }

int main(int argc, char** argv)
{
    auto result = SqlTestFixture::Initialize(argc, argv);
    if (auto const* exitCode = std::get_if<int>(&result))
        return *exitCode;

    std::tie(argc, argv) = std::get<SqlTestFixture::MainProgramArgs>(result);

    return Catch::Session().run(argc, argv);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: resize and clear")
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

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: push_back and pop_back")
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

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: assign")
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

TEST_CASE_METHOD(SqlTestFixture, "SqlFixedString: c_str")
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

TEST_CASE_METHOD(SqlTestFixture, "select: get columns")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("SELECT 42");
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1) == 42);
    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "select: get column (invalid index)")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("SELECT 42");
    REQUIRE(stmt.FetchRow());

    auto const _ = ScopedSqlNullLogger {}; // suppress the error message, we are testing for it

    CHECK_THROWS_AS(stmt.GetColumn<int>(2), std::invalid_argument);
    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "execute bound parameters and select back: VARCHAR, INT")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);

    stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
    stmt.Execute("Alice", "Smith", 50'000);
    stmt.Execute("Bob", "Johnson", 60'000);
    stmt.Execute("Charlie", "Brown", 70'000);

    stmt.ExecuteDirect("SELECT COUNT(*) FROM Employees");
    REQUIRE(stmt.NumColumnsAffected() == 1);
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1) == 3);
    REQUIRE(!stmt.FetchRow());

    stmt.Prepare("SELECT FirstName, LastName, Salary FROM Employees WHERE Salary >= ?");
    REQUIRE(stmt.NumColumnsAffected() == 3);
    stmt.Execute(55'000);

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1) == "Bob");
    REQUIRE(stmt.GetColumn<std::string>(2) == "Johnson");
    REQUIRE(stmt.GetColumn<int>(3) == 60'000);

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1) == "Charlie");
    REQUIRE(stmt.GetColumn<std::string>(2) == "Brown");
    REQUIRE(stmt.GetColumn<int>(3) == 70'000);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "transaction: auto-rollback")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.Connection().TransactionsAllowed());
    CreateEmployeesTable(stmt);

    {
        auto transaction = SqlTransaction { stmt.Connection(), SqlTransactionMode::ROLLBACK };
        stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
        stmt.Execute("Alice", "Smith", 50'000);
        REQUIRE(stmt.Connection().TransactionActive());
    }
    // transaction automatically rolled back

    REQUIRE(!stmt.Connection().TransactionActive());
    stmt.ExecuteDirect("SELECT COUNT(*) FROM Employees");
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1) == 0);
}

TEST_CASE_METHOD(SqlTestFixture, "transaction: auto-commit")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.Connection().TransactionsAllowed());
    CreateEmployeesTable(stmt);

    {
        auto transaction = SqlTransaction { stmt.Connection(), SqlTransactionMode::COMMIT };
        stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
        stmt.Execute("Alice", "Smith", 50'000);
        REQUIRE(stmt.Connection().TransactionActive());
    }
    // transaction automatically committed

    REQUIRE(!stmt.Connection().TransactionActive());
    stmt.ExecuteDirect("SELECT COUNT(*) FROM Employees");
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1) == 1);
}

TEST_CASE_METHOD(SqlTestFixture, "execute binding output parameters (direct)")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    std::string firstName(20, '\0'); // pre-allocation for output parameter strings is important
    std::string lastName(20, '\0');  // ditto
    unsigned int salary {};

    stmt.Prepare("SELECT FirstName, LastName, Salary FROM Employees WHERE Salary = ?");
    stmt.BindOutputColumns(&firstName, &lastName, &salary);
    stmt.Execute(50'000);

    REQUIRE(stmt.FetchRow());
    CHECK(firstName == "Alice");
    CHECK(lastName == "Smith");
    CHECK(salary == 50'000);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "FetchRow can auto-trim string if requested")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
    stmt.Execute("Alice    ", "Smith    ", 50'000);

    SqlTrimmedString firstName { .value = std::string(20, '\0') };
    SqlTrimmedString lastName { .value = std::string(20, '\0') };

    stmt.ExecuteDirect("SELECT FirstName, LastName FROM Employees");
    stmt.BindOutputColumns(&firstName, &lastName);

    REQUIRE(stmt.FetchRow());
    CHECK(firstName.value == "Alice");
    CHECK(lastName.value == "Smith");

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlStatement.ExecuteBatch")
{
    auto stmt = SqlStatement {};

    CreateEmployeesTable(stmt);

    stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");

    // Ensure that the batch insert works with different types of containers
    // clang-format off
    auto const firstNames = std::array { "Alice"sv, "Bob"sv, "Charlie"sv }; // random access STL container (contiguous)
    auto const lastNames = std::list { "Smith"sv, "Johnson"sv, "Brown"sv }; // forward access STL container (non-contiguous)
    unsigned const salaries[3] = { 50'000, 60'000, 70'000 };                // C-style array
    // clang-format on

    stmt.ExecuteBatch(firstNames, lastNames, salaries);

    stmt.ExecuteDirect("SELECT FirstName, LastName, Salary FROM Employees ORDER BY Salary DESC");

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1) == "Charlie");
    REQUIRE(stmt.GetColumn<std::string>(2) == "Brown");
    REQUIRE(stmt.GetColumn<int>(3) == 70'000);

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1) == "Bob");
    REQUIRE(stmt.GetColumn<std::string>(2) == "Johnson");
    REQUIRE(stmt.GetColumn<int>(3) == 60'000);

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1) == "Alice");
    REQUIRE(stmt.GetColumn<std::string>(2) == "Smith");
    REQUIRE(stmt.GetColumn<int>(3) == 50'000);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlStatement.ExecuteBatchNative")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);

    stmt.ExecuteDirect("CREATE TABLE Test (A VARCHAR(8), B REAL, C INTEGER)");

    stmt.Prepare("INSERT INTO Test (A, B, C) VALUES (?, ?, ?)");

    // Ensure that the batch insert works with different types of contiguous containers
    auto const first = std::array<SqlFixedString<8>, 3> { "Hello", "World", "!" };
    auto const second = std::vector { 1.3, 2.3, 3.3 };
    unsigned const third[3] = { 50'000, 60'000, 70'000 };

    stmt.ExecuteBatchNative(first, second, third);

    stmt.ExecuteDirect("SELECT A, B, C FROM Test ORDER BY C DESC");

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "!");
    CHECK_THAT(stmt.GetColumn<double>(2), Catch::Matchers::WithinAbs(3.3, 0.000'001));
    CHECK(stmt.GetColumn<int>(3) == 70'000);

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "World");
    CHECK_THAT(stmt.GetColumn<double>(2), Catch::Matchers::WithinAbs(2.3, 0.000'001));
    CHECK(stmt.GetColumn<int>(3) == 60'000);

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Hello");
    CHECK_THAT(stmt.GetColumn<double>(2), Catch::Matchers::WithinAbs(1.3, 0.000'001));
    CHECK(stmt.GetColumn<int>(3) == 50'000);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlConnection: manual connect")
{
    auto conn = SqlConnection { std::nullopt };
    REQUIRE(!conn.IsAlive());

    conn.Connect(SqlConnection::DefaultConnectInfo());
    REQUIRE(conn.IsAlive());
}

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

TEST_CASE_METHOD(SqlTestFixture, "custom types", "[sql]")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value INT)");

    // check custom type handling for input parameters
    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(CustomType { 42 });

    // check custom type handling for explicitly fetched output columns
    stmt.ExecuteDirect("SELECT Value FROM Test");
    REQUIRE(stmt.FetchRow());
    auto result = stmt.GetColumn<CustomType>(1);
    REQUIRE(result.value == 42);

    // check custom type handling for bound output columns
    result = {};
    stmt.Prepare("SELECT Value FROM Test");
    stmt.BindOutputColumns(&result);
    stmt.Execute();
    REQUIRE(stmt.FetchRow());
    REQUIRE(result.value == (42 | 0x01));
}

TEST_CASE_METHOD(SqlTestFixture, "LastInsertId")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);

    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    // 3 because we inserted 3 rows
    REQUIRE(stmt.LastInsertId() == 3);
}

TEST_CASE_METHOD(SqlTestFixture, "SELECT * FROM Table")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    stmt.ExecuteDirect("SELECT * FROM Employees");

    REQUIRE(stmt.NumColumnsAffected() == 4);

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<int>(1) == 1);
    CHECK(stmt.GetColumn<std::string>(2) == "Alice");
    CHECK(stmt.GetColumn<std::string>(3) == "Smith");
    CHECK(stmt.GetColumn<int>(4) == 50'000);

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<int>(1) == 2);
    CHECK(stmt.GetColumn<std::string>(2) == "Bob");
    CHECK(stmt.GetColumn<std::string>(3) == "Johnson");
    CHECK(stmt.GetColumn<int>(4) == 60'000);
}

TEST_CASE_METHOD(SqlTestFixture, "GetColumn in-place store variant")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    stmt.ExecuteDirect("SELECT FirstName, LastName, Salary FROM Employees");
    REQUIRE(stmt.FetchRow());

    CHECK(stmt.GetColumn<std::string>(1) == "Alice");

    SqlVariant lastName;
    CHECK(stmt.GetColumn(2, &lastName));
    CHECK(std::get<std::string>(lastName.value) == "Smith");

    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    SqlVariant salary;
    CHECK(stmt.GetColumn(3, &salary));
    CHECK(salary.TryGetInt().value_or(0) == 50'000);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: NULL values")
{
    auto stmt = SqlStatement();
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks VARCHAR(50) NULL)");

    stmt.Prepare("INSERT INTO Test (Remarks) VALUES (?)");
    stmt.Execute(SqlNullValue);

    stmt.ExecuteDirect("SELECT Remarks FROM Test");
    REQUIRE(stmt.FetchRow());

    auto const actual = stmt.GetColumn<SqlVariant>(1);
    CHECK(std::holds_alternative<SqlNullType>(actual.value));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlDate")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value DATE NOT NULL)");

    using namespace std::chrono_literals;
    auto const expected = SqlVariant { SqlDate { 2017y, std::chrono::August, 16d } };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expected);

    stmt.ExecuteDirect("SELECT Value FROM Test");
    REQUIRE(stmt.FetchRow());
    auto const actual = stmt.GetColumn<SqlVariant>(1);
    CHECK(std::get<SqlDate>(actual.value) == std::get<SqlDate>(expected.value));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlTime")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect("CREATE TABLE Test (Value TIME NOT NULL)");

    using namespace std::chrono_literals;
    auto const expected = SqlVariant { SqlTime { 12h, 34min, 56s } };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expected);

    stmt.ExecuteDirect("SELECT Value FROM Test");
    REQUIRE(stmt.FetchRow());
    auto const actual = stmt.GetColumn<SqlVariant>(1);

    if (stmt.Connection().ServerType() == SqlServerType::POSTGRESQL)
    {
        WARN("PostgreSQL seems to report SQL_TYPE_DATE here. Skipping check, that would fail otherwise.");
        // TODO: Find out why PostgreSQL reports SQL_TYPE_DATE instead of SQL_TYPE_TIME for SQL column type TIME.
        return;
    }

    CHECK(std::get<SqlTime>(actual.value) == std::get<SqlTime>(expected.value));
}

TEST_CASE_METHOD(SqlTestFixture, "std::optional: InputParameter")
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

TEST_CASE_METHOD(SqlTestFixture, "std::optional: BindOutputColumns")
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

TEST_CASE_METHOD(SqlTestFixture, "std::optional: GetColumn")
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

TEST_CASE_METHOD(SqlTestFixture, "TryGetColumn")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks1 VARCHAR(50) NULL, Remarks2 VARCHAR(50) NULL)");
    stmt.Prepare("INSERT INTO Test (Remarks1, Remarks2) VALUES (?, ?)");
    stmt.Execute("Blurb", SqlNullValue);

    stmt.ExecuteDirect("SELECT Remarks1, Remarks2 FROM Test");
    REQUIRE(stmt.FetchRow());
    auto const actual1 = stmt.TryGetColumn<std::string>(1);
    auto const actual2 = stmt.TryGetColumn<std::string>(2);
    CHECK(actual1.value_or("IS_NULL") == "Blurb");
    CHECK(!actual2.has_value());
}

static std::string MakeLargeText(size_t size)
{
    auto text = std::string(size, '\0');
    std::generate(text.begin(), text.end(), [i = 0]() mutable { return char('A' + (i++ % 26)); });
    return text;
}

TEST_CASE_METHOD(SqlTestFixture, "InputParameter and GetColumn for very large values")
{
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
        std::string actualText; // intentionally an empty string, auto-growing behind the scenes
        stmt.Prepare("SELECT Value FROM Test");
        stmt.BindOutputColumns(&actualText);
        stmt.Execute();
        REQUIRE(stmt.FetchRow());
        REQUIRE(actualText.size() == expectedText.size());
        CHECK(actualText == expectedText);
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlFixedString")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value VARCHAR(8) NOT NULL)");

    auto const expectedValue = SqlFixedString<8> { "Hello " };

    stmt.Prepare("INSERT INTO Test (Value) VALUES (?)");
    stmt.Execute(expectedValue);

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        stmt.ExecuteDirect("SELECT Value FROM Test");
        REQUIRE(stmt.FetchRow());
        auto const actualValue = stmt.GetColumn<SqlFixedString<8>>(1);
        CHECK(actualValue == expectedValue);

        SECTION("Truncated result")
        {
            stmt.ExecuteDirect("SELECT Value FROM Test");
            REQUIRE(stmt.FetchRow());
            auto const truncatedValue = stmt.GetColumn<SqlFixedString<4>>(1);
            auto const truncatedStrView = truncatedValue.substr(0);
            auto const expectedStrView = expectedValue.substr(0, 3);
            CHECK(truncatedStrView == expectedStrView); // "Hel"
        }

        SECTION("Trimmed result")
        {
            stmt.ExecuteDirect("SELECT Value FROM Test");
            REQUIRE(stmt.FetchRow());
            auto const trimmedValue = stmt.GetColumn<SqlTrimmedFixedString<8>>(1);
            CHECK(trimmedValue == "Hello");
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
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlText")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect("CREATE TABLE Test (Value TEXT NOT NULL)");

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
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlDateTime")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect(std::format("CREATE TABLE Test (Value {} NOT NULL)",
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
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: date")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Value DATE NOT NULL)");
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
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: time")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);
    stmt.ExecuteDirect("CREATE TABLE Test (Value TIME NOT NULL)");
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
        std::format("CREATE TABLE Test (Value {}(50) NOT NULL)",
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

    // Fetch and check GetColumn for wide string
    REQUIRE(stmt.FetchRow());
    auto const actualValue = stmt.GetColumn<WideString>(1);
    CHECK(actualValue == inputValue);

    // Bind output column, fetch, and check result in output column for wide string
    WideString actualValue2;
    stmt.BindOutputColumns(&actualValue2);
    REQUIRE(stmt.FetchRow());
    CHECK(actualValue2 == inputValue);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder: SqlGuid", "[SqlDataBinder]")
{
    auto stmt = SqlStatement {};
    UNSUPPORTED_DATABASE(stmt, SqlServerType::ORACLE);

    stmt.ExecuteDirect(std::format("CREATE TABLE Test (id {}, name VARCHAR(50))",
                                   stmt.Connection().Traits().PrimaryKeyGuidColumnType));

    auto const expectedGuid = SqlGuid::Create();

    stmt.Prepare("INSERT INTO Test (id, name) VALUES (?, ?)");
    stmt.Execute(expectedGuid, "Alice");

    // Fetch and check GetColumn for GUID
    stmt.ExecuteDirect("SELECT id, name FROM Test");
    REQUIRE(stmt.FetchRow());
    auto const actualGuid = stmt.GetColumn<SqlGuid>(1);
    CHECK(actualGuid == expectedGuid);

    // Bind output column, fetch, and check result in output column for GUID
    stmt.ExecuteDirect("SELECT id FROM Test");
    SqlGuid actualGuid2;
    stmt.BindOutputColumns(&actualGuid2);
    REQUIRE(stmt.FetchRow());
    CHECK(actualGuid2 == expectedGuid);
    REQUIRE(!stmt.FetchRow());

    // Test SELECT by GUID
    stmt.Prepare("SELECT name FROM Test WHERE id = ?");
    stmt.Execute(expectedGuid);
    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<std::string>(1) == "Alice");
    REQUIRE(!stmt.FetchRow());
}

// NOLINTEND(readability-container-size-empty)
