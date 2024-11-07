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

#include <array>
#include <cstdlib>
#include <list>

// NOLINTBEGIN(readability-container-size-empty)

#if defined(_MSC_VER)
    // Disable the warning C4834: discarding return value of function with 'nodiscard' attribute.
    // Because we are simply testing and demonstrating the library and not using it in production code.
    #pragma warning(disable : 4834)
#endif

using namespace std::string_view_literals;

int main(int argc, char** argv)
{
    auto result = SqlTestFixture::Initialize(argc, argv);
    if (auto const* exitCode = std::get_if<int>(&result))
        return *exitCode;

    std::tie(argc, argv) = std::get<SqlTestFixture::MainProgramArgs>(result);

    return Catch::Session().run(argc, argv);
}

TEST_CASE_METHOD(SqlTestFixture, "select: get columns")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("SELECT 42");
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1) == 42);
    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "move semantics", "[SqlConnection]")
{
    auto a = SqlConnection {};
    CHECK(a.IsAlive());

    auto b = std::move(a);
    CHECK(!a.IsAlive());
    CHECK(b.IsAlive());

    auto c = SqlConnection(std::move(b));
    CHECK(!a.IsAlive());
    CHECK(!b.IsAlive());
    CHECK(c.IsAlive());
}

TEST_CASE_METHOD(SqlTestFixture, "move semantics", "[SqlStatement]")
{
    auto conn = SqlConnection {};

    auto const TestRun = [](SqlStatement& stmt) {
        CHECK(stmt.ExecuteDirectSingle<int>("SELECT 42").value_or(-1) == 42);
    };

    auto a = SqlStatement { conn };
    CHECK(&conn == &a.Connection());
    CHECK(a.Connection().IsAlive());
    TestRun(a);

    auto b = std::move(a);
    CHECK(&conn == &b.Connection());
    CHECK(!a.IsAlive());
    CHECK(b.IsAlive());
    TestRun(b);

    auto c = SqlStatement(std::move(b));
    CHECK(&conn == &c.Connection());
    CHECK(!a.IsAlive());
    CHECK(!b.IsAlive());
    CHECK(c.IsAlive());
    TestRun(c);
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

    CHECK(conn.Connect(SqlConnection::DefaultConnectInfo()));
    CHECK(conn.IsAlive());
}

TEST_CASE_METHOD(SqlTestFixture, "SqlConnection: manual connect (invalid)")
{
    auto conn = SqlConnection { std::nullopt };
    REQUIRE(!conn.IsAlive());

    SqlConnectionDataSource const shouldNotExist { .datasource = "shouldNotExist",
                                                   .username = "shouldNotExist",
                                                   .password = "shouldNotExist" };

    auto const _ = ScopedSqlNullLogger {}; // suppress the error message, we are testing for it
    CHECK(!conn.Connect(shouldNotExist));
    CHECK(!conn.IsAlive());
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

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<int>(1) == 3);
    CHECK(stmt.GetColumn<std::string>(2) == "Charlie");
    CHECK(stmt.GetColumn<std::string>(3) == "Brown");
    CHECK(stmt.GetColumn<int>(4) == 70'000);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "GetNullableColumn")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks1 VARCHAR(50) NULL, Remarks2 VARCHAR(50) NULL)");
    stmt.Prepare("INSERT INTO Test (Remarks1, Remarks2) VALUES (?, ?)");
    stmt.Execute("Blurb", SqlNullValue);

    stmt.ExecuteDirect("SELECT Remarks1, Remarks2 FROM Test");
    REQUIRE(stmt.FetchRow());
    auto const actual1 = stmt.GetNullableColumn<std::string>(1);
    auto const actual2 = stmt.GetNullableColumn<std::string>(2);
    CHECK(actual1.value_or("IS_NULL") == "Blurb");
    CHECK(!actual2.has_value());
}

// NOLINTEND(readability-container-size-empty)
