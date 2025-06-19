add_executable(feed_handler_test feed_handler_test.cpp)
target_link_libraries(feed_handler_test PRIVATE GTest::gtest GTest::gtest_main)

add_executable(venue_simulation_test venue_simulation_test.cpp)
target_link_libraries(venue_simulation_test PRIVATE GTest::gtest GTest::gtest_main)

add_test(NAME feed_handler_test COMMAND feed_handler_test)
add_test(NAME venue_simulation_test COMMAND venue_simulation_test) 