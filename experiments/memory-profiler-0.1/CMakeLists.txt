cmake_minimum_required(VERSION 3.16)
project(fars-memory-profiler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FARS_BUILD_TESTS "Build unit tests" ON)

add_executable(fars-memory-profiler
    src/main.cpp
    src/system_info.cpp
    src/memory_monitor.cpp
    src/storage_bench.cpp
    src/report.cpp
    src/platform_linux.cpp
    src/platform_android.cpp
)

target_include_directories(fars-memory-profiler PRIVATE include)

# Basic warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fars-memory-profiler PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Tests
if(FARS_BUILD_TESTS)
    enable_testing()
    add_executable(fars-profiler-tests
        tests/test_memory_monitor.cpp
        src/memory_monitor.cpp
        src/system_info.cpp
        src/platform_linux.cpp
        src/platform_android.cpp
    )
    target_include_directories(fars-profiler-tests PRIVATE include)
    add_test(NAME profiler_unit_tests COMMAND fars-profiler-tests)
endif()

install(TARGETS fars-memory-profiler RUNTIME DESTINATION bin)