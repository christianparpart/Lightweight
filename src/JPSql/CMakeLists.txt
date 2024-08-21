cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(JPSql LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

if(NOT WIN32)
    find_package(SQLite3 REQUIRED)
    include_directories(${SQLITE3_INCLUDE_DIR})

    # find unixodbc via pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ODBC REQUIRED IMPORTED_TARGET odbc)
    include_directories(${ODBC_INCLUDE_DIRS})
    link_directories(${ODBC_LIBRARY_DIRS})
endif()

set(HEADER_FILES
    Model/Associations/BelongsTo.hpp
    Model/Associations/HasMany.hpp
    Model/Associations/HasOne.hpp
    Model/ColumnType.hpp
    Model/Detail.hpp
    Model/Logger.hpp
    Model/Record.hpp
    Model/Relation.hpp
    Model/StringLiteral.hpp
    Model/Utils.hpp
    SqlConcepts.hpp
    SqlConnection.hpp
    SqlError.hpp
    SqlLogger.hpp
    SqlStatement.hpp
)

set(SOURCE_FILES
    Model/AbstractRecord.cpp
    Model/Logger.cpp
    SqlConnection.cpp
    SqlLogger.cpp
    SqlStatement.cpp
    SqlTransaction.cpp
)

add_library(JPSql STATIC)
add_library(JPSql::JPSql ALIAS JPSql)
target_compile_features(JPSql PUBLIC cxx_std_23)
target_sources(JPSql PRIVATE ${SOURCE_FILES})
# target_sources(JPSql PUBLIC ${HEADER_FILES})

if(CLANG_TIDY_EXE)
    set_target_properties(JPSql PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# target_include_directories(JPSql PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_include_directories(JPSql PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)

if(MSVC)
    target_compile_options(JPSql PRIVATE /W4 /WX)
    target_link_libraries(JPSql PUBLIC odbc32)
else()
    target_compile_options(JPSql PRIVATE -Wall -Wextra -pedantic -Werror)
    #target_link_libraries(JPSql PUBLIC unixodbc) # unixODBC::unixODBC
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(JPSql PUBLIC stdc++exp) # GCC >= 14
    endif()
endif()

if(NOT WIN32)
    target_link_libraries(JPSql ${SQLITE3_LIBRARY})
    target_link_libraries(JPSql ${odbc_LIBRARY})
endif()

enable_testing()
find_package(Catch2 REQUIRED)

add_executable(JPSqlTest)
target_compile_features(JPSqlTest PUBLIC cxx_std_23)
target_link_libraries(JPSqlTest PRIVATE JPSql::JPSql Catch2::Catch2)
target_sources(JPSqlTest PRIVATE ../JPSqlTest/JPSqlTest.cpp)
add_test(NAME JPSqlTest COMMAND JPSqlTest)

add_executable(JPSqlModelTest)
target_compile_features(JPSqlModelTest PUBLIC cxx_std_23)
target_link_libraries(JPSqlModelTest PRIVATE JPSql::JPSql Catch2::Catch2)
target_sources(JPSqlModelTest PRIVATE
    ../JPSqlTest/ModelTest.cpp
    ../JPSqlTest/ModelAssociationsTest.cpp
)
add_test(NAME JPSqlModelTest COMMAND JPSqlModelTest)

