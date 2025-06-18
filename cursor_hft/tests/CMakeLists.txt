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