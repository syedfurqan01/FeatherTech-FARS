cmake_minimum_required(VERSION 3.16)
project(fars-memory-profiler VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(FARS_BUILD_TESTS "Build unit tests" ON)
set(FARS_COMMON_SOURCES
    src/platform/proc_parser.cpp
    src/file_reader.cpp
    src/process_memory.cpp
    src/system_memory.cpp
    src/sampling_engine.cpp
    src/storage_benchmark.cpp
)
add_executable(fars-memory-profiler src/main.cpp ${FARS_COMMON_SOURCES})
target_include_directories(fars-memory-profiler PRIVATE include)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(fars-memory-profiler PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(FARS_BUILD_TESTS)
    enable_testing()
    add_executable(fars-profiler-tests tests/test_all.cpp ${FARS_COMMON_SOURCES})
    target_include_directories(fars-profiler-tests PRIVATE include)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(fars-profiler-tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME all_tests COMMAND fars-profiler-tests)
endif()
install(TARGETS fars-memory-profiler RUNTIME DESTINATION bin)
