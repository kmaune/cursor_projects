cmake_minimum_required(VERSION 3.20)
project(HFTTradingSystem CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# M1 Pro optimizations
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address")
endif()

# Include directories
include_directories(include)

# Find required packages
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Add timing library
add_library(hft_timing
    src/timing/hft_timer.cpp
)

target_include_directories(hft_timing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add timing tests
add_executable(hft_timing_test
    tests/timing/hft_timer_test.cpp
)

target_link_libraries(hft_timing_test
    PRIVATE
        hft_timing
        GTest::GTest
        GTest::Main
)

# Add timing benchmarks
add_executable(hft_timing_benchmark
    benchmarks/timing/hft_timer_benchmark.cpp
)

target_link_libraries(hft_timing_benchmark
    PRIVATE
        hft_timing
        benchmark::benchmark
        benchmark::benchmark_main
)

# Add test command
add_test(NAME hft_timing_test COMMAND hft_timing_test)

# Add a simple executable for testing
add_executable(test_setup src/test_setup.cpp)

