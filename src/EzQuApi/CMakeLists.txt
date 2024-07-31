set(HEADER_FILES
    SqlConcepts.hpp
    SqlConnection.hpp
    SqlError.hpp
    SqlStatement.hpp
)

set(SOURCE_FILES
    SqlConnection.cpp
    SqlStatement.cpp
)

add_library(EzQuApi STATIC)
add_library(EzQuApi::EzQuApi ALIAS EzQuApi)
target_compile_features(EzQuApi PUBLIC cxx_std_23)
target_sources(EzQuApi PRIVATE ${SOURCE_FILES})
# target_sources(EzQuApi PUBLIC ${HEADER_FILES})

# find_package(unixODBC REQUIRED)

if(CLANG_TIDY_EXE)
    set_target_properties(EzQuApi PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# target_include_directories(EzQuApi PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_include_directories(EzQuApi PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)

if(MSVC)
    target_compile_options(EzQuApi PRIVATE /W4 /WX)
    target_link_libraries(EzQuApi PUBLIC odbc32)
else()
    target_compile_options(EzQuApi PRIVATE -Wall -Wextra -pedantic -Werror)
    target_link_libraries(EzQuApi PUBLIC odbc) # unixODBC::unixODBC
endif()

find_package(Catch2 REQUIRED)
add_executable(EzQuApiTest)
target_compile_features(EzQuApiTest PUBLIC cxx_std_23)
target_link_libraries(EzQuApiTest PRIVATE EzQuApi::EzQuApi Catch2::Catch2)
target_sources(EzQuApiTest PRIVATE EzQuApiTests.cpp)
add_test(NAME EzQuApiTest COMMAND EzQuApiTest)
