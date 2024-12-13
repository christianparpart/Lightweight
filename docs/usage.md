# Examples

# Configure connection to the database

To connect to the database you need to provide connection string that library uses to establish connection and you can check if it alive in the following way
```c
SqlConnection::SetDefaultConnectionString(SqlConnectionString {
    .value = std::format("DRIVER=SQLite3;Database={}", SqlConnectionString::SanitizePwd("Database.sqlite")) });

auto sqlConnection = SqlConnection();
if (!sqlConnection.IsAlive())
{
    std::println("Failed to connect to the database: {}",
                 SqlErrorInfo::fromConnectionHandle(sqlConnection.NativeHandle()));
    std::abort();
}
```

# Sql Query

You can directly make a call to the database using `ExecuteDirect` function, for example
```c
auto stmt = SqlStatement {};
stmt.ExecuteDirect(R"("SELECT "a", "b", "c" FROM "That" ORDER BY "That"."b" DESC)"));
while (stmt.FetchRow())
{
   auto a = stmt.GetColumn<int>(1);
   auto b = stmt.GetColumn<int>(2);
   auto c = stmt.GetColumn<int>(3);
   std::println("{}|{}|{}", a, b,c);
}

```

Or you can constuct statement using `SqlQueryBuilder` for different databases
```c
auto sqliteQueryBuilder = SqlQueryBuilder(formatter);
auto const sqlQuery =  sqliteQueryBuilder.FromTable("That")
                .Select()
                .Fields("a", "b")
                .Field("c")
                .OrderBy(SqlQualifiedTableColumnName { .tableName = "That", .columnName = "b" },
                         SqlResultOrdering::DESCENDING)
                .All()

```

For more info see `SqlQuery` and `SqlQueryFormatter` documentation
