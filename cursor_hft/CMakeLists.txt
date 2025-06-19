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

# Add memory management library (header-only)
add_library(hft_memory INTERFACE)
target_include_directories(hft_memory INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hft_memory INTERFACE 
    hft_timing  # ObjectPool depends on timing
)

# Add market data library (header-only)
add_library(hft_market_data INTERFACE)
target_include_directories(hft_market_data INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hft_market_data INTERFACE 
    hft_timing
    hft_memory
    hft_messaging
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

# Add object pool tests
add_executable(hft_object_pool_test
    tests/memory/object_pool_test.cpp
)
target_link_libraries(hft_object_pool_test
    PRIVATE
        hft_memory
        hft_timing
        gtest_main
        gtest
)

# Add treasury market data tests
add_executable(hft_treasury_yield_test
    tests/timing/treasury_market_data_yield_test.cpp
)
target_link_libraries(hft_treasury_yield_test
    PRIVATE
        hft_market_data
        hft_timing
        gtest_main
        gtest
)

add_executable(hft_treasury_pool_test
    tests/memory/treasury_market_data_pool_test.cpp
)
target_link_libraries(hft_treasury_pool_test
    PRIVATE
        hft_market_data
        hft_memory
        gtest_main
        gtest
)

add_executable(hft_treasury_ring_buffer_test
    tests/messaging/treasury_market_data_ring_buffer_test.cpp
)
target_link_libraries(hft_treasury_ring_buffer_test
    PRIVATE
        hft_market_data
        hft_messaging
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

# Add object pool benchmarks
add_executable(hft_object_pool_benchmark
    benchmarks/memory/object_pool_benchmark.cpp
)
target_link_libraries(hft_object_pool_benchmark
    PRIVATE
        hft_memory
        hft_timing
        benchmark::benchmark
        benchmark::benchmark_main
)

# Add treasury market data benchmarks
add_executable(hft_treasury_yield_benchmark
    benchmarks/timing/treasury_market_data_yield_benchmark.cpp
)
target_link_libraries(hft_treasury_yield_benchmark
    PRIVATE
        hft_market_data
        hft_timing
        benchmark::benchmark
        benchmark::benchmark_main
)

add_executable(hft_treasury_pool_benchmark
    benchmarks/memory/treasury_market_data_pool_benchmark.cpp
)
target_link_libraries(hft_treasury_pool_benchmark
    PRIVATE
        hft_market_data
        hft_memory
        benchmark::benchmark
        benchmark::benchmark_main
)

add_executable(hft_treasury_ring_buffer_benchmark
    benchmarks/messaging/treasury_market_data_ring_buffer_benchmark.cpp
)
target_link_libraries(hft_treasury_ring_buffer_benchmark
    PRIVATE
        hft_market_data
        hft_messaging
        benchmark::benchmark
        benchmark::benchmark_main
)

# Enable testing
enable_testing()

# Add test commands
add_test(NAME hft_timing_test COMMAND hft_timing_test)
add_test(NAME hft_spsc_ring_buffer_test COMMAND hft_spsc_ring_buffer_test)
add_test(NAME hft_object_pool_test COMMAND hft_object_pool_test)
add_test(NAME hft_treasury_yield_test COMMAND hft_treasury_yield_test)
add_test(NAME hft_treasury_pool_test COMMAND hft_treasury_pool_test)
add_test(NAME hft_treasury_ring_buffer_test COMMAND hft_treasury_ring_buffer_test)

# Add a simple executable for testing
add_executable(test_setup src/test_setup.cpp)
target_link_libraries(test_setup PRIVATE hft_timing hft_messaging)

install(DIRECTORY include/hft/memory/ DESTINATION include/hft/memory)
install(DIRECTORY include/hft/market_data/ DESTINATION include/hft/market_data)
