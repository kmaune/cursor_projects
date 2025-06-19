base) kmaune@mac build% ctest --output-on-failure
Test project /Users/kmaune/Desktop/coding/ai_tool_tests/cursor/cursor_hft/build
    Start 1: hft_feed_handler_test
1/7 Test #1: hft_feed_handler_test ............***Failed    0.01 sec
Running main() from /Users/kmaune/Desktop/coding/ai_tool_tests/cursor/cursor_hft/build/_deps/googletest-src/googletest/src/gtest_main.cc
[==========] Running 11 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 11 tests from FeedHandler
[ RUN      ] FeedHandler.StructAlignment
[       OK ] FeedHandler.StructAlignment (0 ms)
[ RUN      ] FeedHandler.ChecksumValidation
[       OK ] FeedHandler.ChecksumValidation (1 ms)
[ RUN      ] FeedHandler.TickParsingAccuracy
[       OK ] FeedHandler.TickParsingAccuracy (0 ms)
[ RUN      ] FeedHandler.TradeParsingAccuracy
/Users/kmaune/Desktop/coding/ai_tool_tests/cursor/cursor_hft/tests/market_data/feed_handler_test.cpp:92: Failure
Expected equality of these values:
  trade.trade_size
    Which is: 4637001174145302528
  3000u
    Which is: 3000

[  FAILED  ] FeedHandler.TradeParsingAccuracy (0 ms)
[ RUN      ] FeedHandler.BatchParsing
[       OK ] FeedHandler.BatchParsing (0 ms)
[ RUN      ] FeedHandler.FeedHandlerWorkflow
[       OK ] FeedHandler.FeedHandlerWorkflow (0 ms)
[ RUN      ] FeedHandler.QualityControlDuplicatesAndSequence
/Users/kmaune/Desktop/coding/ai_tool_tests/cursor/cursor_hft/tests/market_data/feed_handler_test.cpp:122: Failure
Expected equality of these values:
  stats.sequence_gaps
    Which is: 3
  1u
    Which is: 1

[  FAILED  ] FeedHandler.QualityControlDuplicatesAndSequence (0 ms)
[ RUN      ] FeedHandler.MessageNormalization
[       OK ] FeedHandler.MessageNormalization (0 ms)
[ RUN      ] FeedHandler.ObjectPoolIntegration
[       OK ] FeedHandler.ObjectPoolIntegration (0 ms)
[ RUN      ] FeedHandler.RingBufferIntegration
[       OK ] FeedHandler.RingBufferIntegration (0 ms)
[ RUN      ] FeedHandler.ErrorHandlingEdgeCases
[       OK ] FeedHandler.ErrorHandlingEdgeCases (0 ms)
[----------] 11 tests from FeedHandler (1 ms total)

[----------] Global test environment tear-down
[==========] 11 tests from 1 test suite ran. (1 ms total)
[  PASSED  ] 9 tests.
[  FAILED  ] 2 tests, listed below:
[  FAILED  ] FeedHandler.TradeParsingAccuracy
[  FAILED  ] FeedHandler.QualityControlDuplicatesAndSequence

 2 FAILED TESTS

    Start 2: hft_timing_test
2/7 Test #2: hft_timing_test ..................   Passed    0.09 sec
    Start 3: hft_spsc_ring_buffer_test
3/7 Test #3: hft_spsc_ring_buffer_test ........   Passed    0.08 sec
    Start 4: hft_object_pool_test
4/7 Test #4: hft_object_pool_test .............   Passed    4.39 sec
    Start 5: hft_treasury_yield_test
5/7 Test #5: hft_treasury_yield_test ..........   Passed    0.00 sec
    Start 6: hft_treasury_pool_test
6/7 Test #6: hft_treasury_pool_test ...........   Passed    0.00 sec
    Start 7: hft_treasury_ring_buffer_test
7/7 Test #7: hft_treasury_ring_buffer_test ....   Passed    0.00 sec

86% tests passed, 1 tests failed out of 7

Total Test time (real) =   4.59 sec

The following tests FAILED:
          1 - hft_feed_handler_test (Failed)
Errors while running CTest
(base) kmaune@mac build%
