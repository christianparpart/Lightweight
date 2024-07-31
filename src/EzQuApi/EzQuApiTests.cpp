#include <EzQuApi/SqlConnection.hpp>
#include <EzQuApi/SqlStatement.hpp>

#include <cstdlib>
#include <format>
#include <iostream>

// This is playground for testing the library

int main(int argc, char const* argv[])
{
    auto sqlConnection = SqlConnection {};

    if (argc != 4)
    {
        std::cerr << std::format("Usage: {} <server> <username> <password>\n", argv[0]);
        return EXIT_FAILURE;
    }

    sqlConnection.Connect(argv[1], argv[2], argv[3]); // Configure it via launch.json when debugging via Visual Studio 

    if (!sqlConnection.IsSuccess())
    {
        std::cerr << std::format("Failed to connect to the database.\n");
        return EXIT_FAILURE;
    }

    std::cout << "SQL DBMS    : " << sqlConnection.ServerName() << "\n";
    std::cout << "SQL DB name : " << sqlConnection.DatabaseName() << "\n";

    auto stmt = SqlStatement { sqlConnection };

    // Perform table creation
    stmt.ExecuteDirect("DROP TABLE IF EXISTS AAA_TEST_Employees");
    stmt.ExecuteDirect(R"SQL(
        CREATE TABLE AAA_TEST_Employees (
            EmployeeID INT IDENTITY(1,1) PRIMARY KEY,
            FirstName VARCHAR(50) NOT NULL,
            LastName VARCHAR(50),
            Salary INT NOT NULL
        );
    )SQL");

    // Insert some data
    stmt.Prepare("INSERT INTO AAA_TEST_Employees (FirstName, LastName, Salary) VALUES (?, ?, ?)");
    stmt.Execute("Alice", "Smith", 50'000);
    stmt.Execute("Bob", "Johnson", 60'000);
    stmt.Execute("Charlie", "Brown", 70'000);
    stmt.Execute("David", "White", 80'000);

    // Query the data count
    stmt.ExecuteDirect("SELECT COUNT(*) FROM AAA_TEST_Employees");
    if (stmt.FetchRow())
        std::cout << std::format("We have {} total records in test table\n", stmt.GetColumn<int>(1));

    // Query the data items
    stmt.Prepare("SELECT EmployeeID, FirstName, LastName, Salary FROM AAA_TEST_Employees WHERE Salary >= ?");
    stmt.Execute(55'000);
    std::cout << std::format("Affected rows {}, columns {}\n", stmt.NumRowsAffected(), stmt.NumColumnsAffected());

    while (stmt.FetchRow())
    {
        auto const id = stmt.GetColumn<unsigned long long>(1);
        auto const firstName = stmt.GetColumn<std::string>(2);
        auto const lastName = stmt.GetColumn<std::string>(3);
        auto const salary = stmt.GetColumn<int>(4);
        std::cout << std::format("Employee #{}: \"{}\" \"{}\" with salary {}\n", id, firstName, lastName, salary);
    }

    // Cleanup
    // stmt.ExecuteDirect("DROP TABLE IF EXISTS AAA_TEST_Employees");
    return 0;
}
