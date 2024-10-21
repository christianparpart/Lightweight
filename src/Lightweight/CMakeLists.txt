cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(Lightweight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

if(NOT WIN32)
    find_package(SQLite3 REQUIRED)
    include_directories(${SQLITE3_INCLUDE_DIR})

    # find unixODBC via pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ODBC REQUIRED odbc)
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
    SqlConnectInfo.hpp
    SqlConnection.hpp
    SqlError.hpp
    SqlLogger.hpp
    SqlQuery.hpp
    SqlQuery/Core.hpp
    SqlQuery/Delete.hpp
    SqlQuery/Insert.hpp
    SqlQuery/Select.hpp
    SqlQuery/Update.hpp
    SqlQueryFormatter.hpp
    SqlSchema.hpp
    SqlScopedTraceLogger.hpp
    SqlStatement.hpp
)

set(SOURCE_FILES
    Model/AbstractRecord.cpp
    Model/Logger.cpp
    SqlConnectInfo.cpp
    SqlConnection.cpp
    SqlError.cpp
    SqlLogger.cpp
    SqlQuery.cpp
    SqlQuery/Select.cpp
    SqlQueryFormatter.cpp
    SqlSchema.cpp
    SqlStatement.cpp
    SqlTransaction.cpp
)

add_library(Lightweight STATIC)
add_library(Lightweight::Lightweight ALIAS Lightweight)
target_compile_features(Lightweight PUBLIC cxx_std_23)
target_sources(Lightweight PRIVATE ${SOURCE_FILES})
#target_sources(Lightweight PUBLIC ${HEADER_FILES})

if(CLANG_TIDY_EXE)
    set_target_properties(Lightweight PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# target_include_directories(Lightweight PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_include_directories(Lightweight PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)

if(MSVC)
    target_compile_options(Lightweight PRIVATE /W4 /WX)
    target_compile_options(Lightweight PRIVATE /MP)
    target_link_libraries(Lightweight PUBLIC odbc32)
else()
    target_compile_options(Lightweight PRIVATE -Wall -Wextra -pedantic -Werror)
    target_compile_options(Lightweight PUBLIC ${ODBC_CFLAGS})
    target_link_libraries(Lightweight PUBLIC ${ODBC_LDFLAGS})

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(Lightweight PUBLIC stdc++exp) # GCC >= 14
    endif()
endif()
