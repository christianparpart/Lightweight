# JPSql

**JPSql** is a thin C++ ODBC wrapper for easy and fast raw database access.

This is not meant to replace DABase's SQL, but to complement it with a more explicit and performant API.

## Features

- **Easy to use** - simple, expressive and intuitive API
- **Performance** - do as little as possible, and as much as necessary - efficiency is key
- **Extensible** - support for custom data types for writing to and reading from columns
- **Exception safe** - no need to worry about resource management
- **Open Collaboration** - code directly integrated into the main project
- **Monad-like** - simple error handling with `std::expected`-like API

## Non-Goals

- Feature creeping (ODBC is a huge API, we are not going to wrap everything)
- No intent to replace DABase's SQL API. High level access is not the goal of this library.

## C++ Language requirements

This library a little bit of more modern C++ language and library features in order to be more expressive and efficient.

- C++23 (`std::expected`, `std::stacktrace`, lambda templates expressions)
- C++20 (`std::source_location`, `std::error_code`, `operator<=>`, `std::format()`, designated initializers, concepts, ranges)
- C++17 (fold expressions, `std::string_view`, `std::optional`, `std::variant`, `std::apply`)

## Supported Databases

- Microsoft SQL
- PostgreSQL (untested)
- Oracle (untested)
- SQLite (only for internal testing purposes)

## Using SQLite for testing

You need to have the SQLite3 ODBC driver for SQLite installed.

- ODBC driver download URL: http://www.ch-werner.de/sqliteodbc/
- Example connection string: `DRIVER={SQLite3 ODBC Driver};Database=file::memory:`

### SQLite ODBC driver installation on other operating systems

```sh
# Fedora Linux
sudo dnf install sqliteodbc

# Ubuntu Linux
sudo apt install sqliteodbc

# macOS
arch -arm64 brew install sqliteodbc
```

- sqliteODBC Documentation: http://www.ch-werner.de/sqliteodbc/html/index.html
- Example connection string: `DRIVER=SQLite3;Database=file::memory:`

## Open TODOs

- [x] Initial connection API
- [x] Initial prepared statement API
- [x] Proper error handling
- [x] Support transactions
- [x] Support default connection, such that an SqlStatement can be created without requiring a connection
- [x] (PERFORMANCE) Make SQLGetData write directly into the string buffer
- [x] (PERFORMANCE) Support binding bulk data (many rows) to a prepared statement, fallback to single row iteration if not supported
- [x] (PERFORMANCE) SqlConnectionPool: SqlConnection pooling through static accessor `SqlConnection::CreateDefaultConnection()` to default connection (auto-invalidate on default connect info change)
- [x] (TESTING) Test suite for the API with basic test coverage (using SQLite)
- [x] (FEATURE) Support binding output parameters for convenience (and performance)
- [x] (COMPATIBILITY) Support auto-trimming CHAR(n) columns by introducing a new string type (`SqlTrimmedString`)
- [x] (FEATURE) Support query and performance metrics logging
- [x] (FEATURE) Support custom C++ data types (via `SqlDataBinder<T>`-specialization)
- [x] (FEATURE) Support SQL data types: timestamp (utilizing chrono standard library)
- [x] (FEATURE) Support SQL data type: `std::chrono::year_month_day` (date)
- [x] (FEATURE) Support SQL data type: `std::chrono::hh_mm_ss` (time of day)
- [ ] (FEATURE) Handle NULL values in result sets (partially implemented already)
- [x] (CI) integrate unit tests into CI pipeline
- [x] (CI) test compile for 32bit and 64bit using MSVC
- [x] (CI) Create test case for overly large strings to be written and read back
- [ ] (CI) test using MS-SQL, PostgreSQL, Oracle (?)
- [x] (LASTRADA) when Lastrada connects to the database, or changes the database, we must update the default connection in JPSql as well
- [ ] (LASTRADA) Provide data bindings for `RNString`, `CString`, `QString` via `SqlOutputStringTraits<T>`
