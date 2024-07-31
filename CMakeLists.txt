cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(EzQuApi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

if(DEFINED MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
    add_compile_options(/utf-8)
    add_compile_options(/nologo)
endif()

enable_testing()
add_subdirectory(src/EzQuApi)
