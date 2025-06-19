# Find Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Add messaging tests
add_executable(messaging_test
    messaging/message_test.cpp
    messaging/ring_buffer_test.cpp
    messaging/message_dispatcher_test.cpp
    messaging/event_loop_test.cpp
)

target_link_libraries(messaging_test
    PRIVATE
    hft_messaging
    gtest_main
    gtest
)

# Add test command
add_test(NAME messaging_test COMMAND messaging_test)

add_executable(object_pool_test memory/object_pool_test.cpp)
target_link_libraries(object_pool_test PRIVATE hft_timing)
add_test(NAME object_pool_test COMMAND object_pool_test)

add_executable(treasury_market_data_pool_test memory/treasury_market_data_pool_test.cpp)
target_link_libraries(treasury_market_data_pool_test gtest_main)
add_test(NAME treasury_market_data_pool_test COMMAND treasury_market_data_pool_test)

add_executable(treasury_market_data_ring_buffer_test messaging/treasury_market_data_ring_buffer_test.cpp)
target_link_libraries(treasury_market_data_ring_buffer_test gtest_main)
add_test(NAME treasury_market_data_ring_buffer_test COMMAND treasury_market_data_ring_buffer_test)

add_executable(treasury_market_data_yield_test timing/treasury_market_data_yield_test.cpp)
target_link_libraries(treasury_market_data_yield_test gtest_main)
add_test(NAME treasury_market_data_yield_test COMMAND treasury_market_data_yield_test) 