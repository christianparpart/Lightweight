# Lightweight, an ODBC SQL API for C++23

**Lightweight** is a thin and modern C++ ODBC wrapper for **easy** and **fast** raw database access.

## Goals

- **Easy to use** - simple, expressive and intuitive API
- **Production ready** - targeting production grade systems
- **Performance** - do as little as possible, and as much as necessary - **Zero overhead abstraction** is a key design requirement.
- **Extensible** - support for custom data types for writing to and reading from columns
- **Resource aware** - efficient resource management and exception safety

## Non-Goals

- Feature creeping (ODBC is a huge API, we are not going to wrap everything)
- No intend to support non-ODBC connectors directly, in order to keep the codebase simple and focused

## Features

- Customizable logging infrastructure with support for trace and performance timing logging
- Flexible SQL dialect-agnostic query builder API
- Support for custom data types

## Supported platforms

Only ODBC is supported, so it should work on any platform that has an ODBC driver and
a modern enough C++ compiler.

- Windows (Visual Studio 2022, toolkit v143)
- Linux (GCC 14, Clang 19)

## Supported Databases

- Microsoft SQL
- PostgreSQL
- SQLite3
- Oracle database (work in progress)

## C++ Language requirements

This library a little bit of more modern C++ language and library features in order to be more expressive and efficient.

- C++23 (`std::stacktrace`, lambda templates expressions)
- C++20 (`std::source_location`, `std::error_code`, `operator<=>`, `std::format()`, designated initializers, concepts, ranges)
- C++17 (fold expressions, `std::string_view`, `std::optional`, `std::variant`, `std::apply`)

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
