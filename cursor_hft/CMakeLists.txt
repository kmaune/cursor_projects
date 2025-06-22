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

# Add trading library (header-only)
add_library(hft_trading INTERFACE)
target_include_directories(hft_trading INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hft_trading INTERFACE 
    hft_timing
    hft_memory
    hft_messaging
    hft_market_data
)

# Add strategy library (header-only)
add_library(hft_strategy INTERFACE)
target_include_directories(hft_strategy INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(hft_strategy INTERFACE 
    hft_timing
    hft_memory
    hft_messaging
    hft_market_data
    hft_trading
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

# Add order book tests
add_executable(hft_order_book_test
    tests/trading/order_book_test.cpp
)
target_link_libraries(hft_order_book_test
    PRIVATE
        hft_trading
        hft_market_data
        hft_memory
        hft_messaging
        hft_timing
        gtest_main
        gtest
)

# Add strategy functional tests
add_executable(hft_simple_market_maker_test
    tests/strategy/test_simple_market_maker.cpp
)
target_link_libraries(hft_simple_market_maker_test
    PRIVATE
        hft_strategy
        hft_trading
        hft_market_data
        hft_memory
        hft_messaging
        hft_timing
        gtest_main
        gtest
)

# Add strategy performance tests (separate from functional tests)
add_executable(hft_simple_market_maker_performance_test
    tests/strategy/test_simple_market_maker_performance.cpp
)
target_link_libraries(hft_simple_market_maker_performance_test
    PRIVATE
        hft_strategy
        hft_trading
        hft_market_data
        hft_memory
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

# Add order book benchmarks
add_executable(hft_order_book_benchmark
    benchmarks/trading/order_book_benchmark.cpp
)
target_link_libraries(hft_order_book_benchmark
    PRIVATE
        hft_trading
        hft_market_data
        hft_memory
        hft_messaging
        hft_timing
        benchmark::benchmark
        benchmark::benchmark_main
)

# Add strategy benchmarks
add_executable(hft_simple_market_maker_benchmark
    benchmarks/strategy/simple_market_maker_benchmark.cpp
)
target_link_libraries(hft_simple_market_maker_benchmark
    PRIVATE
        hft_strategy
        hft_trading
        hft_market_data
        hft_memory
        hft_messaging
        hft_timing
        benchmark::benchmark
        benchmark::benchmark_main
)

# Feed Handler Test
add_executable(hft_feed_handler_test tests/market_data/feed_handler_test.cpp)
target_link_libraries(hft_feed_handler_test 
    PRIVATE 
        hft_market_data hft_timing hft_memory hft_messaging
        gtest_main gtest)
add_test(NAME hft_feed_handler_test COMMAND hft_feed_handler_test)

# Feed Handler Benchmark  
add_executable(hft_feed_handler_benchmark benchmarks/market_data/feed_handler_benchmark.cpp)
target_link_libraries(hft_feed_handler_benchmark 
    PRIVATE 
        hft_market_data hft_timing hft_memory hft_messaging
        benchmark::benchmark benchmark::benchmark_main)

# End-to-End System Integration Benchmark
add_executable(hft_end_to_end_benchmark benchmarks/system/end_to_end_benchmark.cpp)
target_link_libraries(hft_end_to_end_benchmark 
    PRIVATE 
        hft_market_data hft_trading hft_timing hft_memory hft_messaging
        benchmark::benchmark benchmark::benchmark_main)

# Enable testing
enable_testing()

# Add test commands
add_test(NAME hft_timing_test COMMAND hft_timing_test)
add_test(NAME hft_spsc_ring_buffer_test COMMAND hft_spsc_ring_buffer_test)
add_test(NAME hft_object_pool_test COMMAND hft_object_pool_test)
add_test(NAME hft_treasury_yield_test COMMAND hft_treasury_yield_test)
add_test(NAME hft_treasury_pool_test COMMAND hft_treasury_pool_test)
add_test(NAME hft_treasury_ring_buffer_test COMMAND hft_treasury_ring_buffer_test)
add_test(NAME hft_order_book_test COMMAND hft_order_book_test)
add_test(NAME hft_simple_market_maker_test COMMAND hft_simple_market_maker_test)

# Performance tests are separate and not run by default in CI
# Run manually with: ./hft_simple_market_maker_performance_test
# Or with ctest: ctest -R "performance" -V

# Add a simple executable for testing
add_executable(test_setup src/test_setup.cpp)
target_link_libraries(test_setup PRIVATE hft_timing hft_messaging)

# Benchmark automation system
set(HFT_BENCHMARK_TARGETS
    hft_timing_benchmark
    hft_spsc_ring_buffer_benchmark
    hft_object_pool_benchmark
    hft_treasury_yield_benchmark
    hft_treasury_pool_benchmark
    hft_treasury_ring_buffer_benchmark
    hft_order_book_benchmark
    hft_simple_market_maker_benchmark
    hft_feed_handler_benchmark
    hft_end_to_end_benchmark
)

# Add custom target to run all benchmarks
add_custom_target(benchmark
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/simple_benchmark_validator.py --quick
    DEPENDS ${HFT_BENCHMARK_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running all HFT benchmarks..."
)

# Add custom target to validate performance against baselines
add_custom_target(benchmark-validate
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/simple_benchmark_validator.py
    DEPENDS ${HFT_BENCHMARK_TARGETS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Validating benchmark performance against targets..."
)

# Add benchmarks as CTest tests for CI integration
add_test(
    NAME hft_performance_validation
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/simple_benchmark_validator.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(hft_performance_validation PROPERTIES 
    TIMEOUT 300
    LABELS "performance;benchmark"
)

install(DIRECTORY include/hft/memory/ DESTINATION include/hft/memory)
install(DIRECTORY include/hft/market_data/ DESTINATION include/hft/market_data)
