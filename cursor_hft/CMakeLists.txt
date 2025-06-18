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

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Find benchmark
find_package(benchmark REQUIRED)
find_package(Threads REQUIRED)

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

# Add messaging library (header-only for now)
add_library(hft_messaging INTERFACE)
target_include_directories(hft_messaging INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hft_messaging INTERFACE 
    Threads::Threads
    hft_timing  # Messaging depends on timing
)

# Add timing tests
add_executable(hft_timing_test
    tests/timing/hft_timer_test.cpp
)

target_link_libraries(hft_timing_test
    PRIVATE
        hft_timing
        gtest_main
        gtest
)

# Add SPSC ring buffer tests
add_executable(hft_spsc_ring_buffer_test
    tests/messaging/spsc_ring_buffer_test.cpp
)

target_link_libraries(hft_spsc_ring_buffer_test
    PRIVATE
        hft_messaging
        hft_timing
        gtest_main
        gtest
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

# Add SPSC ring buffer benchmarks
add_executable(hft_spsc_ring_buffer_benchmark
    benchmarks/messaging/spsc_ring_buffer_benchmark.cpp
)

target_link_libraries(hft_spsc_ring_buffer_benchmark
    PRIVATE
        hft_messaging
        hft_timing
        benchmark::benchmark
        benchmark::benchmark_main
)

# Enable testing
enable_testing()

# Add test commands
add_test(NAME hft_timing_test COMMAND hft_timing_test)
add_test(NAME hft_spsc_ring_buffer_test COMMAND hft_spsc_ring_buffer_test)

# Add a simple executable for testing
add_executable(test_setup src/test_setup.cpp)
target_link_libraries(test_setup PRIVATE hft_timing hft_messaging)
