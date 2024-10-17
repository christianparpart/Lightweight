#include "../Lightweight/SqlScopedTraceLogger.hpp"
#include "Utils.hpp"

#include <Lightweight/SqlComposedQuery.hpp>
#include <Lightweight/SqlConnection.hpp>
#include <Lightweight/SqlQueryFormatter.hpp>
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
#include <ostream>

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

    struct finally
    {
        ~finally()
        {
            SqlLogger::GetLogger().OnStats(SqlConnection::Stats());
        }
    } _;

    return Catch::Session().run(argc, argv);
}

namespace
{

void CreateEmployeesTable(SqlStatement& stmt, std::source_location sourceLocation = std::source_location::current())
{
    stmt.ExecuteDirect(std::format(R"SQL(CREATE TABLE Employees (
                                                         EmployeeID {},
                                                         FirstName VARCHAR(50) NOT NULL,
                                                         LastName VARCHAR(50),
                                                         Salary INT NOT NULL
                                                     );
                                                    )SQL",
                                   stmt.Connection().Traits().PrimaryKeyAutoIncrement),
                       sourceLocation);
}

void FillEmployeesTable(SqlStatement& stmt)
{
    stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
    stmt.Execute("Alice", "Smith", 50'000);
    stmt.Execute("Bob", "Johnson", 60'000);
    stmt.Execute("Charlie", "Brown", 70'000);
}

} // namespace

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

TEST_CASE_METHOD(SqlTestFixture, "connection pool reusage", "[sql]")
{
    // auto-instanciating an SqlConnection
    auto const id1 = [] {
        auto connection = SqlConnection {};
        return connection.ConnectionId();
    }();

    // Explicitly passing a borrowed SqlConnection
    auto const id2 = [] {
        auto conn = SqlConnection {};
        auto stmt = SqlStatement { conn };
        return stmt.Connection().ConnectionId();
    }();
    CHECK(id1 == id2);

    // &&-created SqlConnections are reused
    auto const id3 = SqlConnection().ConnectionId();
    CHECK(id1 == id3);

    // Explicit constructor passing SqlConnectInfo always creates a new SqlConnection
    auto const id4 = SqlConnection(SqlConnection::DefaultConnectInfo()).ConnectionId();
    CHECK(id1 != id4);
}

struct CustomType
{
    int value;
};

template <>
struct SqlDataBinder<CustomType>
{
    static SQLRETURN InputParameter(SQLHSTMT hStmt, SQLUSMALLINT column, CustomType const& value) noexcept
    {
        return SqlDataBinder<int>::InputParameter(hStmt, column, value.value);
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

    static SQLRETURN GetColumn(SQLHSTMT hStmt, SQLUSMALLINT column, CustomType* result, SQLLEN* indicator) noexcept
    {
        return SqlDataBinder<int>::GetColumn(hStmt, column, &result->value, indicator);
    }

    static constexpr int PostProcess(int value) noexcept
    {
        return value |= 0x01;
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
    stmt.GetColumn(2, &lastName);
    CHECK(std::get<std::string>(lastName) == "Smith");

    SqlVariant salary;
    stmt.GetColumn(3, &salary);
    CHECK(std::get<int>(salary) == 50'000);
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
    CHECK(std::holds_alternative<SqlNullType>(actual));
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
    CHECK(std::get<SqlDate>(actual) == std::get<SqlDate>(expected));
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlTime")
{
    auto stmt = SqlStatement {};
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

    CHECK(std::get<SqlTime>(actual) == std::get<SqlTime>(expected));
}

TEST_CASE_METHOD(SqlTestFixture, "std::optional: NULL values")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks VARCHAR(50) NULL)");
    stmt.Prepare("INSERT INTO Test (Remarks) VALUES (?)");
    stmt.Execute(SqlNullValue);

    stmt.ExecuteDirect("SELECT Remarks FROM Test");
    REQUIRE(stmt.FetchRow());

    auto const actual = stmt.GetColumn<std::optional<std::string>>(1);
    CHECK(!actual.has_value());
}

TEST_CASE_METHOD(SqlTestFixture, "std::optional: via BindOutputColumns")
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

TEST_CASE_METHOD(SqlTestFixture, "std::optional: non-NULL values via GetColumn")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("CREATE TABLE Test (Remarks VARCHAR(50) NULL)");

    stmt.Prepare("INSERT INTO Test (Remarks) VALUES (?)");
    stmt.Execute("Blurb");

    stmt.ExecuteDirect("SELECT Remarks FROM Test");
    REQUIRE(stmt.FetchRow());

    auto const actual = stmt.GetColumn<std::optional<std::string>>(1);
    REQUIRE(actual.has_value());
    CHECK(*actual == "Blurb");
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
        stmt.GetColumn(1, &actualText);
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

struct QueryExpectations
{
    std::string_view sqlite;
    std::string_view sqlServer;
};

void checkSqlQueryBuilder(SqlComposedQuery const& sqlQuery,
                          QueryExpectations const& expectations,
                          std::source_location const& location = std::source_location::current())
{
    INFO(std::format("Test source location: {}:{}", location.file_name(), location.line()));

    auto const& sqliteFormatter = SqlQueryFormatter::Sqlite();
    auto const& sqlServerFormatter = SqlQueryFormatter::SqlServer();

    CHECK(sqlQuery.ToSql(sqliteFormatter) == expectations.sqlite);
    CHECK(sqlQuery.ToSql(sqlServerFormatter) == expectations.sqlServer);
};

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Count")
{
    checkSqlQueryBuilder(SqlQueryBuilder::From("Table").Count(),
                         QueryExpectations {
                             .sqlite = "SELECT COUNT(*) FROM \"Table\"",
                             .sqlServer = "SELECT COUNT(*) FROM \"Table\"",
                         });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.All")
{
    checkSqlQueryBuilder(SqlQueryBuilder::From("That").Select("a", "b").Select("c").GroupBy("a").OrderBy("b").All(),
                         QueryExpectations {
                             .sqlite = R"(SELECT "a", "b", "c" FROM "That" GROUP BY "a" ORDER BY "b" ASC)",
                             .sqlServer = R"(SELECT "a", "b", "c" FROM "That" GROUP BY "a" ORDER BY "b" ASC)",
                         });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.First")
{
    checkSqlQueryBuilder(SqlQueryBuilder::From("That").Select("field1").OrderBy("id").First(),
                         QueryExpectations {
                             .sqlite = R"(SELECT "field1" FROM "That" ORDER BY "id" ASC LIMIT 1)",
                             .sqlServer = R"(SELECT TOP 1 "field1" FROM "That" ORDER BY "id" ASC)",
                         });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Range")
{
    checkSqlQueryBuilder(
        SqlQueryBuilder::From("That").Select("foo", "bar").OrderBy("id").Range(200, 50),
        QueryExpectations {
            .sqlite = R"(SELECT "foo", "bar" FROM "That" ORDER BY "id" ASC LIMIT 50 OFFSET 200)",
            .sqlServer = R"(SELECT "foo", "bar" FROM "That" ORDER BY "id" ASC OFFSET 200 ROWS FETCH NEXT 50 ROWS ONLY)",
        });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.Delete", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(SqlQueryBuilder::From("That").Where("foo", 42).Where("bar", "baz").Delete(),
                         QueryExpectations {
                             .sqlite = R"(DELETE FROM "That" WHERE "foo" = 42 AND "bar" = 'baz')",
                             .sqlServer = R"(DELETE FROM "That" WHERE "foo" = 42 AND "bar" = 'baz')",
                         });
}

TEST_CASE_METHOD(SqlTestFixture, "SqlQueryBuilder.WhereInRange", "[SqlQueryBuilder]")
{
    checkSqlQueryBuilder(SqlQueryBuilder::From("That").Where("foo", std::vector { 1, 2, 3 }).Delete(),
                         QueryExpectations {
                             .sqlite = R"(DELETE FROM "That" WHERE "foo" IN (1, 2, 3))",
                             .sqlServer = R"(DELETE FROM "That" WHERE "foo" IN (1, 2, 3))",
                         });
}
