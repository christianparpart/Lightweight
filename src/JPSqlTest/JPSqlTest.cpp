#include "JPSqlTestUtils.hpp"

#include <JPSql/SqlConnection.hpp>
#include <JPSql/SqlStatement.hpp>
#include <JPSql/SqlTransaction.hpp>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

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

namespace
{

class SqlTestFixture
{
  protected:
    void CreateEmployeesTable(SqlStatement& stmt, std::source_location sourceLocation = std::source_location::current())
    {
        REQUIRE(stmt.ExecuteDirect(std::format(R"SQL(CREATE TABLE Employees (
                                                         EmployeeID {},
                                                         FirstName VARCHAR(50) NOT NULL,
                                                         LastName VARCHAR(50),
                                                         Salary INT NOT NULL
                                                     );
                                                    )SQL",
                                               stmt.Connection().Traits().PrimaryKeyAutoIncrement),
                                   sourceLocation));
    }

    void FillEmployeesTable(SqlStatement& stmt)
    {
        REQUIRE(stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)"));
        REQUIRE(stmt.Execute("Alice", "Smith", 50'000));
        REQUIRE(stmt.Execute("Bob", "Johnson", 60'000));
        REQUIRE(stmt.Execute("Charlie", "Brown", 70'000));
    }

  public:
    SqlTestFixture()
    {
        // (customize or enable if needed) SqlLogger::SetLogger(SqlLogger::TraceLogger());
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
    return Catch::Session().run(argc, argv);
}

TEST_CASE_METHOD(SqlTestFixture, "ServerType")
{
    // For now, this program is configured to use SQLite, as it is the easiest to setup and tests
    REQUIRE(SqlConnection().ServerType() == SqlServerType::SQLITE);
}

TEST_CASE_METHOD(SqlTestFixture, "select: get columns")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("SELECT 42").value();
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1).value() == 42);
    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "select: get column (invalid index)")
{
    auto stmt = SqlStatement {};
    stmt.ExecuteDirect("SELECT 42").value();
    REQUIRE(stmt.FetchRow());

    auto const _ = ScopedSqlNullLogger {}; // suppress the error message, we are testing for it

    auto const result = stmt.GetColumn<int>(2);
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == SqlError::FAILURE); // SQL_ERROR
    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "execute bound parameters and select back: VARCHAR, INT")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);

    REQUIRE(stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)"));
    REQUIRE(stmt.Execute("Alice", "Smith", 50'000));
    REQUIRE(stmt.Execute("Bob", "Johnson", 60'000));
    REQUIRE(stmt.Execute("Charlie", "Brown", 70'000));

    REQUIRE(stmt.ExecuteDirect("SELECT COUNT(*) FROM Employees"));
    REQUIRE(stmt.NumColumnsAffected().value() == 1);
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1).value() == 3);
    REQUIRE(!stmt.FetchRow());

    REQUIRE(stmt.Prepare("SELECT FirstName, LastName, Salary FROM Employees WHERE Salary >= ?"));
    REQUIRE(stmt.NumColumnsAffected().value() == 3);
    REQUIRE(stmt.Execute(55'000));

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1).value() == "Bob");
    REQUIRE(stmt.GetColumn<std::string>(2).value() == "Johnson");
    REQUIRE(stmt.GetColumn<int>(3).value() == 60'000);

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1).value() == "Charlie");
    REQUIRE(stmt.GetColumn<std::string>(2).value() == "Brown");
    REQUIRE(stmt.GetColumn<int>(3).value() == 70'000);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "transaction: auto-rollback")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.Connection().TransactionsAllowed());
    CreateEmployeesTable(stmt);

    {
        auto transaction = SqlTransaction { stmt.Connection(), SqlTransactionMode::ROLLBACK };
        REQUIRE(stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)"));
        REQUIRE(stmt.Execute("Alice", "Smith", 50'000));
        REQUIRE(stmt.Connection().TransactionActive());
    }
    // transaction automatically rolled back

    REQUIRE(!stmt.Connection().TransactionActive());
    REQUIRE(stmt.ExecuteDirect("SELECT COUNT(*) FROM Employees"));
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1).value() == 0);
}

TEST_CASE_METHOD(SqlTestFixture, "transaction: auto-commit")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.Connection().TransactionsAllowed());
    CreateEmployeesTable(stmt);

    {
        auto transaction = SqlTransaction { stmt.Connection(), SqlTransactionMode::COMMIT };
        REQUIRE(stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)"));
        REQUIRE(stmt.Execute("Alice", "Smith", 50'000));
        REQUIRE(stmt.Connection().TransactionActive());
    }
    // transaction automatically committed

    REQUIRE(!stmt.Connection().TransactionActive());
    REQUIRE(stmt.ExecuteDirect("SELECT COUNT(*) FROM Employees"));
    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<int>(1).value() == 1);
}

TEST_CASE_METHOD(SqlTestFixture, "execute binding output parameters (direct)")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    std::string firstName(20, '\0'); // pre-allocation for output parameter strings is important
    std::string lastName(20, '\0');  // ditto
    unsigned int salary {};

    REQUIRE(stmt.Prepare("SELECT FirstName, LastName, Salary FROM Employees WHERE Salary = ?"));
    REQUIRE(stmt.BindOutputColumns(&firstName, &lastName, &salary));
    REQUIRE(stmt.Execute(50'000));

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
    REQUIRE(stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)"));
    REQUIRE(stmt.Execute("Alice    ", "Smith    ", 50'000));

    SqlTrimmedString firstName { .value = std::string(20, '\0') };
    SqlTrimmedString lastName { .value = std::string(20, '\0') };

    REQUIRE(stmt.ExecuteDirect("SELECT FirstName, LastName FROM Employees"));
    REQUIRE(stmt.BindOutputColumns(&firstName, &lastName));

    REQUIRE(stmt.FetchRow());
    CHECK(firstName.value == "Alice");
    CHECK(lastName.value == "Smith");

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "batch insert")
{
    auto stmt = SqlStatement {};

    CreateEmployeesTable(stmt);

    REQUIRE(stmt.Prepare("INSERT INTO Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)"));

    // Ensure that the batch insert works with different types of containers
    auto const firstNames = std::array { "Alice"sv, "Bob"sv, "Charlie"sv }; // random access STL container
    auto const lastNames = std::list { "Smith"sv, "Johnson"sv, "Brown"sv }; // forward access STL container
    unsigned const salaries[3] = { 50'000, 60'000, 70'000 };                // C-style array

    REQUIRE(stmt.ExecuteBatch(firstNames, lastNames, salaries));

    REQUIRE(stmt.ExecuteDirect("SELECT FirstName, LastName, Salary FROM Employees ORDER BY Salary DESC"));

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1).value() == "Charlie");
    REQUIRE(stmt.GetColumn<std::string>(2).value() == "Brown");
    REQUIRE(stmt.GetColumn<int>(3).value() == 70'000);

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1).value() == "Bob");
    REQUIRE(stmt.GetColumn<std::string>(2).value() == "Johnson");
    REQUIRE(stmt.GetColumn<int>(3).value() == 60'000);

    REQUIRE(stmt.FetchRow());
    REQUIRE(stmt.GetColumn<std::string>(1).value() == "Alice");
    REQUIRE(stmt.GetColumn<std::string>(2).value() == "Smith");
    REQUIRE(stmt.GetColumn<int>(3).value() == 50'000);

    REQUIRE(!stmt.FetchRow());
}

TEST_CASE_METHOD(SqlTestFixture, "connection pool reusage", "[sql]")
{
    // auto-instanciating an SqlConnection
    auto const id1 = [] {
        auto stmt = SqlStatement {};
        return stmt.Connection().ConnectionId();
    }();

    // Explicitly passing a borrowed SqlConnection
    auto const id2 = [] {
        auto conn = SqlConnection {};
        auto stmt = SqlStatement { conn };
        return stmt.Connection().ConnectionId();
    }();
    REQUIRE(id1 == id2);

    // &&-created SqlConnections are reused
    auto const id3 = SqlConnection().ConnectionId();
    REQUIRE(id1 == id3);

    // Explicit constructor passing SqlConnectInfo always creates a new SqlConnection
    auto const id4 = SqlConnection(SqlConnection::DefaultConnectInfo()).ConnectionId();
    REQUIRE(id1 != id4);
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
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value INT)"));

    // check custom type handling for input parameters
    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(CustomType { 42 }));

    // check custom type handling for explicitly fetched output columns
    REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
    REQUIRE(stmt.FetchRow());
    auto result = stmt.GetColumn<CustomType>(1).value();
    REQUIRE(result.value == 42);

    // check custom type handling for bound output columns
    result = {};
    REQUIRE(stmt.Prepare("SELECT Value FROM Test"));
    REQUIRE(stmt.BindOutputColumns(&result));
    REQUIRE(stmt.Execute());
    REQUIRE(stmt.FetchRow());
    REQUIRE(result.value == (42 | 0x01));
}

TEST_CASE_METHOD(SqlTestFixture, "LastInsertId")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    // 3 because we inserted 3 rows
    REQUIRE(stmt.LastInsertId().value() == 3);
}

TEST_CASE_METHOD(SqlTestFixture, "SELECT * FROM Table")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    REQUIRE(stmt.ExecuteDirect("SELECT * FROM Employees"));

    REQUIRE(stmt.NumColumnsAffected().value_or(0) == 4);

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<int>(1).value() == 1);
    CHECK(stmt.GetColumn<std::string>(2).value() == "Alice");
    CHECK(stmt.GetColumn<std::string>(3).value() == "Smith");
    CHECK(stmt.GetColumn<int>(4).value() == 50'000);

    REQUIRE(stmt.FetchRow());
    CHECK(stmt.GetColumn<int>(1).value() == 2);
    CHECK(stmt.GetColumn<std::string>(2).value() == "Bob");
    CHECK(stmt.GetColumn<std::string>(3).value() == "Johnson");
    CHECK(stmt.GetColumn<int>(4).value() == 60'000);
}

TEST_CASE_METHOD(SqlTestFixture, "GetColumn in-place store variant")
{
    auto stmt = SqlStatement {};
    CreateEmployeesTable(stmt);
    FillEmployeesTable(stmt);

    REQUIRE(stmt.ExecuteDirect("SELECT FirstName, LastName, Salary FROM Employees"));
    REQUIRE(stmt.FetchRow());

    CHECK(stmt.GetColumn<std::string>(1).value() == "Alice");

    SqlVariant lastName;
    REQUIRE(stmt.GetColumn(2, &lastName));
    CHECK(std::get<std::string>(lastName) == "Smith");

    SqlVariant salary;
    REQUIRE(stmt.GetColumn(3, &salary));
    CHECK(std::get<int>(salary) == 50'000);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlDate")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value DATE NOT NULL)"));

    using namespace std::chrono_literals;
    auto const expected = SqlDate { 2017y, std::chrono::August, 16d };

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expected));

    REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
    REQUIRE(stmt.FetchRow());
    auto const actual = stmt.GetColumn<SqlVariant>(1).value();
    CHECK(std::get<SqlDate>(actual) == expected);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlTime")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value TIME NOT NULL)"));

    using namespace std::chrono_literals;
    auto const expected = SqlTime { 12h, 34min, 56s };

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expected));

    REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
    REQUIRE(stmt.FetchRow());
    auto const actual = stmt.GetColumn<SqlVariant>(1).value();
    CHECK(std::get<SqlTime>(actual) == expected);
}

TEST_CASE_METHOD(SqlTestFixture, "SqlVariant: SqlTimestamp")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value TIMESTAMP NOT NULL)"));

    using namespace std::chrono_literals;
    auto const expected = SqlTimestamp { 2017y, std::chrono::August, 16d, 17h, 30min, 45s };

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expected));

    REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
    REQUIRE(stmt.FetchRow());
    auto const actual = stmt.GetColumn<SqlVariant>(1).value();
    CHECK(std::get<SqlTimestamp>(actual) == expected);
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
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Text (Value TEXT)"));
    auto const expectedText = MakeLargeText(8 * 1024);
    REQUIRE(stmt.Prepare("INSERT INTO Text (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expectedText));

    SECTION("check handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Text"));
        REQUIRE(stmt.FetchRow());
        CHECK(stmt.GetColumn<std::string>(1).value() == expectedText);
    }

    SECTION("check handling for explicitly fetched output columns (in-place store)")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Text"));
        REQUIRE(stmt.FetchRow());
        std::string actualText;
        REQUIRE(stmt.GetColumn(1, &actualText));
        CHECK(actualText == expectedText);
    }

    SECTION("check handling for bound output columns")
    {
        std::string actualText; // intentionally an empty string, auto-growing behind the scenes
        REQUIRE(stmt.Prepare("SELECT Value FROM Text"));
        REQUIRE(stmt.BindOutputColumns(&actualText));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        REQUIRE(actualText.size() == expectedText.size());
        CHECK(actualText == expectedText);
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlFixedString")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value VARCHAR(8) NOT NULL)"));

    auto const expectedValue = SqlFixedString<8> { "Hello " };

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expectedValue));

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
        REQUIRE(stmt.FetchRow());
        auto const actualValue = stmt.GetColumn<SqlFixedString<8>>(1).value();
        CHECK(actualValue == expectedValue);

        SECTION("Truncated result")
        {
            REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
            REQUIRE(stmt.FetchRow());
            auto const truncatedValue = stmt.GetColumn<SqlFixedString<4>>(1).value();
            auto const truncatedStrView = truncatedValue.substr(0);
            auto const expectedStrView = expectedValue.substr(0, 3);
            CHECK(truncatedStrView == expectedStrView); // "Hel"
        }

        SECTION("Trimmed result")
        {
            REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
            REQUIRE(stmt.FetchRow());
            auto const trimmedValue = stmt.GetColumn<SqlTrimmedFixedString<8>>(1).value();
            CHECK(trimmedValue == "Hello");
        }
    }

    SECTION("check custom type handling for bound output columns")
    {
        REQUIRE(stmt.Prepare("SELECT Value FROM Test"));
        auto actualValue = SqlFixedString<8> {};
        REQUIRE(stmt.BindOutputColumns(&actualValue));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == expectedValue);
    }

    SECTION("check custom type handling for bound output columns (trimmed)")
    {
        REQUIRE(stmt.Prepare("SELECT Value FROM Test"));
        auto actualValue = SqlTrimmedFixedString<8> {};
        REQUIRE(stmt.BindOutputColumns(&actualValue));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == "Hello");
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlText")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value TEXT NOT NULL)"));

    using namespace std::chrono_literals;
    auto const expectedValue = SqlText { "Hello, World!" };

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expectedValue));

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
        REQUIRE(stmt.FetchRow());
        auto const actualValue = stmt.GetColumn<SqlText>(1).value();
        CHECK(actualValue == expectedValue);
    }

    SECTION("check custom type handling for bound output columns")
    {
        REQUIRE(stmt.Prepare("SELECT Value FROM Test"));
        auto actualValue = SqlText {};
        REQUIRE(stmt.BindOutputColumns(&actualValue));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == expectedValue);
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: SqlDateTime")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value DATETIME NOT NULL)"));

    using namespace std::chrono_literals;
    auto const expectedValue = SqlDateTime(2017y, std::chrono::August, 16d, 17h, 30min, 45s);

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expectedValue));

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
        REQUIRE(stmt.FetchRow());
        auto const actualValue = stmt.GetColumn<SqlDateTime>(1).value();
        CHECK(actualValue == expectedValue);
    }

    SECTION("check custom type handling for bound output columns")
    {
        REQUIRE(stmt.Prepare("SELECT Value FROM Test"));
        auto actualValue = SqlDateTime {};
        REQUIRE(stmt.BindOutputColumns(&actualValue));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        CHECK(actualValue == expectedValue);
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: timestamp")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Timestamps (Value TIMESTAMP NOT NULL)"));

    using namespace std::chrono_literals;
    auto const timestamp = SqlTimestamp(2017y, std::chrono::August, 16d, 17h, 30min, 45s);

    REQUIRE(stmt.Prepare("INSERT INTO Timestamps (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(timestamp));

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Timestamps"));
        REQUIRE(stmt.FetchRow());
        auto const result = stmt.GetColumn<SqlTimestamp>(1).value();
        REQUIRE(result == timestamp);
    }

    SECTION("check custom type handling for bound output columns")
    {
        REQUIRE(stmt.Prepare("SELECT Value FROM Timestamps"));
        auto result = SqlTimestamp {};
        REQUIRE(stmt.BindOutputColumns(&result));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        REQUIRE(result == timestamp);
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: date")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value DATE NOT NULL)"));
    using namespace std::chrono_literals;
    auto const expected = SqlDate { std::chrono::year_month_day { 2017y, std::chrono::August, 16d } };

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expected));

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlDate>(1).value();
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlDate>(1).value();
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for bound output columns")
    {
        REQUIRE(stmt.Prepare("SELECT Value FROM Test"));
        auto actual = SqlDate {};
        REQUIRE(stmt.BindOutputColumns(&actual));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        REQUIRE(actual == expected);
    }
}

TEST_CASE_METHOD(SqlTestFixture, "SqlDataBinder for SQL type: time")
{
    auto stmt = SqlStatement {};
    REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value TIME NOT NULL)"));
    using namespace std::chrono_literals;
    auto const expected = SqlTime(12h, 34min, 56s);

    REQUIRE(stmt.Prepare("INSERT INTO Test (Value) VALUES (?)"));
    REQUIRE(stmt.Execute(expected));

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlTime>(1).value();
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for explicitly fetched output columns")
    {
        REQUIRE(stmt.ExecuteDirect("SELECT Value FROM Test"));
        REQUIRE(stmt.FetchRow());
        auto const actual = stmt.GetColumn<SqlTime>(1).value();
        REQUIRE(actual == expected);
    }

    SECTION("check custom type handling for bound output columns")
    {
        REQUIRE(stmt.Prepare("SELECT Value FROM Test"));
        auto actual = SqlTime {};
        REQUIRE(stmt.BindOutputColumns(&actual));
        REQUIRE(stmt.Execute());
        REQUIRE(stmt.FetchRow());
        REQUIRE(actual == expected);
    }
}

// TODO: do we want this? LIKE THAT?
// #include <JPSql/SqlQueryBuilder.hpp>
// TEST_CASE_METHOD(SqlTestFixture, "SqlBasicQueryBuilder")
// {
// //     auto stmt = SqlStatement {};
// //     REQUIRE(stmt.ExecuteDirect("CREATE TABLE Test (Value TEXT NOT NULL, Number INTEGER NOT NULL)"));
// //     REQUIRE(stmt.Prepare("INSERT INTO Test (Value, Number) VALUES (?, ?)"));
// //     REQUIRE(stmt.Execute("Alice", 42));
// //     REQUIRE(stmt.Execute("Bob", 43));
// //     REQUIRE(stmt.Execute("Charlie", 44));
// //     REQUIRE(stmt.Execute("David", 45));
//
//     CHECK(SqlBasicQueryBuilder::From("Test").Select("*").Where("Number", 43).First()
//           == "SELECT * FROM \"Test\" WHERE \"Number\" = ? LIMIT 1");
//
//     CHECK(SqlBasicQueryBuilder::From("Test").Select("foo").Where("Number", 43).First()
//           == "SELECT \"foo\" FROM \"Test\" WHERE \"Number\" = ? LIMIT 1");
//
//     CHECK(SqlBasicQueryBuilder::From("Test").Select("foo", "bar").Where("Number", 43).First()
//           == "SELECT \"foo\", \"bar\" FROM \"Test\" WHERE \"Number\" = ? LIMIT 1");
// }
