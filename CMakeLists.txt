cmake_minimum_required(VERSION 3.11.4)
project(RVPT)

option(RVPT_BUILD_DEMO "Build the demo executable" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_package(Vulkan REQUIRED)

add_subdirectory(external)

# Determine whether we're compiling with clang++
string(FIND "${CMAKE_CXX_COMPILER}" "clang++" RVPT_COMPILER_CLANGPP)
if(RVPT_COMPILER_CLANGPP GREATER -1)
    set(RVPT_COMPILER_CLANGPP 1)
else()
    set(RVPT_COMPILER_CLANGPP 0)
endif()

add_library(rvpt_compiler_warnings INTERFACE)

target_compile_options(rvpt_compiler_warnings
        INTERFACE
        $<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,${RVPT_COMPILER_CLANGPP}>:
        -Wall
        -Wextra
        -pedantic-errors
        -Wconversion
        -Wsign-conversion>
        $<$<CXX_COMPILER_ID:MSVC>:
        /W4>
        )

add_subdirectory(util)

add_subdirectory(rvpt)

if (RVPT_BUILD_DEMO)
add_subdirectory(demo)
endif()