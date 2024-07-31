# EzQuApi

EzQuApi is a modern C++ ODBC wrapper for easy and convinient database access.

This is a research and development project.

**EXPECT MASTER BRANCH TO BE FORCE PUSHED DURING EARLY PRIVATE DEVELOPMENT**

## Goals

- Do as little as possible, and as much as necessary.
- Convinient and easy to use.
- Performance
- Transparent or easy recovery from broken connections

## Non-Goals

- Feature creeping (ODBC is a huge API, we are not going to wrap everything)

## Requirements & Dependencies

- C++23
- CMake
- ODBC (unixODBC, iODBC, etc.)

## Open TODOs

- [ ] proper error handling
- [ ] decide on the use of `std::error_code` and `std::error_condition`
- [ ] ensure it works on x86 and x86-64 for Windows OS and Linux (x86-64, ARM64)
- [ ] how to make it testible without too much ODBC system integration requirements?
