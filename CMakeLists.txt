cmake_minimum_required(VERSION 3.21)

project(LinuPaint
    VERSION 1.0.0
    DESCRIPTION "Classic Paint for Linux"
    HOMEPAGE_URL "https://github.com/linupaint/linupaint"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LINUPAINT_APP_ID "io.github.linupaint.LinuPaint")

option(LINUPAINT_BUILD_APP "Build the Qt application" ON)
option(LINUPAINT_BUILD_TESTS "Build unit tests" ON)
option(LINUPAINT_BUILD_FUZZERS "Build libFuzzer targets (clang only)" OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/raster)
add_subdirectory(src/core)

if(LINUPAINT_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(LINUPAINT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LINUPAINT_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
