# JPSql

**JPSql** is a thin C++ ODBC wrapper for **easy** and **fast** raw database access.

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
- PostgreSQL
- SQLite
- Oracle database (untested)

## Using SQLite for testing on Windows operating system

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
