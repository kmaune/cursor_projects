#include <gtest/gtest.h>
#include <chrono>
#include <vector>

#include "hft/strategy/treasury_quote_manager.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"

using namespace hft::strategy;
using namespace hft::market_data;
using namespace hft::trading;

class TreasuryQuoteManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pool with sufficient capacity
        order_pool_ = std::make_unique<hft::memory::ObjectPool<TreasuryOrder, 1000>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_);
        
        // Initialize quote manager
        quote_manager_ = std::make_unique<TreasuryQuoteManager>(*order_pool_, *order_book_);
    }

    void TearDown() override {
        quote_manager_.reset();
        order_book_.reset();
        order_pool_.reset();
    }

    // Helper function to create quote update request
    static QuoteUpdateRequest create_quote_request(TreasuryType instrument,
                                          double bid_price, double ask_price,
                                          uint64_t bid_size = 1000000, uint64_t ask_size = 1000000) {
        return QuoteUpdateRequest(
            instrument,
            Price32nd::from_decimal(bid_price),
            Price32nd::from_decimal(ask_price),
            bid_size,
            ask_size
        );
    }

    std::unique_ptr<hft::memory::ObjectPool<TreasuryOrder, 1000>> order_pool_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<TreasuryQuoteManager> quote_manager_;
};

// Test basic quote validation
TEST_F(TreasuryQuoteManagerTest, BasicQuoteValidation) {
    // Valid quote aligned to 32nds
    auto valid_request = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125); // 102 16/32, 102 17/32
    auto result = quote_manager_->validate_quote(valid_request);
    EXPECT_EQ(result, QuoteValidationResult::VALID);
    
    // Invalid price increment (not aligned to 32nds)
    auto invalid_increment = create_quote_request(TreasuryType::Note_10Y, 102.501, 102.531);
    result = quote_manager_->validate_quote(invalid_increment);
    EXPECT_EQ(result, QuoteValidationResult::INVALID_PRICE_INCREMENT);
    
    // Invalid price order (bid >= ask)
    auto invalid_order = create_quote_request(TreasuryType::Note_10Y, 102.6, 102.5);
    result = quote_manager_->validate_quote(invalid_order);
    EXPECT_EQ(result, QuoteValidationResult::INVALID_PRICE_ORDER);
    
    // Invalid spread (too narrow)
    auto narrow_spread = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.500001);
    result = quote_manager_->validate_quote(narrow_spread);
    EXPECT_EQ(result, QuoteValidationResult::INVALID_SPREAD);
}

// Test 32nds pricing compliance
TEST_F(TreasuryQuoteManagerTest, ThirtySecondsCompliance) {
    // Test various 32nds alignments
    std::vector<double> valid_prices = {
        102.0,      // 102 0/32
        102.03125,  // 102 1/32
        102.0625,   // 102 2/32
        102.15625,  // 102 5/32
        102.5,      // 102 16/32
        102.96875   // 102 31/32
    };
    
    for (auto bid_price : valid_prices) {
        for (auto ask_price : valid_prices) {
            if (ask_price > bid_price + 0.03125) { // Ensure minimum spread
                auto request = create_quote_request(TreasuryType::Note_10Y, bid_price, ask_price);
                auto result = quote_manager_->validate_quote(request);
                EXPECT_EQ(result, QuoteValidationResult::VALID) 
                    << "Failed for bid: " << bid_price << ", ask: " << ask_price;
            }
        }
    }
    
    // Test invalid 32nds alignments
    std::vector<double> invalid_prices = {
        102.001,    // Not aligned
        102.015,    // Not aligned
        102.033,    // Not aligned
        102.0625001 // Close but not exact
    };
    
    for (auto invalid_price : invalid_prices) {
        auto request = create_quote_request(TreasuryType::Note_10Y, invalid_price, invalid_price + 0.1);
        auto result = quote_manager_->validate_quote(request);
        EXPECT_EQ(result, QuoteValidationResult::INVALID_PRICE_INCREMENT)
            << "Should fail for price: " << invalid_price;
    }
}

// Test quote size limits
TEST_F(TreasuryQuoteManagerTest, QuoteSizeLimits) {
    // Too small
    auto small_size = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125, 50000, 50000);
    auto result = quote_manager_->validate_quote(small_size);
    EXPECT_EQ(result, QuoteValidationResult::INVALID_SIZE);
    
    // Too large
    auto large_size = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125, 100000000, 100000000);
    result = quote_manager_->validate_quote(large_size);
    EXPECT_EQ(result, QuoteValidationResult::INVALID_SIZE);
    
    // Valid sizes
    std::vector<uint64_t> valid_sizes = {100000, 1000000, 10000000, 50000000};
    for (auto size : valid_sizes) {
        auto valid_request = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125, size, size);
        result = quote_manager_->validate_quote(valid_request);
        EXPECT_EQ(result, QuoteValidationResult::VALID) << "Failed for size: " << size;
    }
}

// Test quote processing performance
TEST_F(TreasuryQuoteManagerTest, QuoteProcessingPerformance) {
    auto request = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125);
    
    constexpr int num_iterations = 1000;
    std::vector<bool> results;
    std::vector<uint64_t> latencies;
    results.reserve(num_iterations);
    latencies.reserve(num_iterations);
    
    for (int i = 0; i < num_iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        bool success = quote_manager_->process_quote_update(request);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        
        results.push_back(success);
        latencies.push_back(latency);
        
        // Clean up for next iteration
        quote_manager_->cancel_quotes(TreasuryType::Note_10Y);
    }
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    auto median = latencies[num_iterations / 2];
    auto p95 = latencies[static_cast<size_t>(num_iterations * 0.95)];
    
    EXPECT_LE(median, 300);  // 300ns target
    EXPECT_LE(p95, 500);     // 95th percentile under 500ns
    
    std::cout << "Quote Processing Latency Statistics:\n";
    std::cout << "  Median: " << median << "ns\n";
    std::cout << "  95th percentile: " << p95 << "ns\n";
}

// Test quote update and state management
TEST_F(TreasuryQuoteManagerTest, QuoteStateManagement) {
    auto request = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125, 2000000, 2000000);
    
    // Process initial quote
    bool success = quote_manager_->process_quote_update(request);
    EXPECT_TRUE(success);
    
    // Verify quote is active
    auto current_quote = quote_manager_->get_current_quote(TreasuryType::Note_10Y);
    EXPECT_TRUE(current_quote.is_active);
    EXPECT_EQ(current_quote.bid_price.to_decimal(), 102.5);
    EXPECT_EQ(current_quote.ask_price.to_decimal(), 102.53125);
    EXPECT_EQ(current_quote.bid_size, 2000000);
    EXPECT_EQ(current_quote.ask_size, 2000000);
    
    // Update quote
    auto updated_request = create_quote_request(TreasuryType::Note_10Y, 102.46875, 102.5, 1500000, 1500000);
    success = quote_manager_->process_quote_update(updated_request);
    EXPECT_TRUE(success);
    
    // Verify quote was updated
    current_quote = quote_manager_->get_current_quote(TreasuryType::Note_10Y);
    EXPECT_TRUE(current_quote.is_active);
    EXPECT_EQ(current_quote.bid_price.to_decimal(), 102.46875);
    EXPECT_EQ(current_quote.ask_price.to_decimal(), 102.5);
    EXPECT_EQ(current_quote.bid_size, 1500000);
    EXPECT_EQ(current_quote.ask_size, 1500000);
}

// Test quote cancellation
TEST_F(TreasuryQuoteManagerTest, QuoteCancellation) {
    auto request = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125);
    
    // Process quote
    bool success = quote_manager_->process_quote_update(request);
    EXPECT_TRUE(success);
    
    // Verify quote is active
    auto current_quote = quote_manager_->get_current_quote(TreasuryType::Note_10Y);
    EXPECT_TRUE(current_quote.is_active);
    
    // Cancel quote
    success = quote_manager_->cancel_quotes(TreasuryType::Note_10Y);
    EXPECT_TRUE(success);
    
    // Verify quote is inactive
    current_quote = quote_manager_->get_current_quote(TreasuryType::Note_10Y);
    EXPECT_FALSE(current_quote.is_active);
    EXPECT_EQ(current_quote.bid_order_id, 0);
    EXPECT_EQ(current_quote.ask_order_id, 0);
}

// Test multi-instrument quote management
TEST_F(TreasuryQuoteManagerTest, MultiInstrumentQuoting) {
    std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M, TreasuryType::Bill_6M, TreasuryType::Note_2Y,
        TreasuryType::Note_5Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y
    };
    
    std::array<std::pair<double, double>, 6> prices = {
        {99.9, 99.96875},     // 3M Bill
        {99.8, 99.84375},     // 6M Bill
        {101.5, 101.53125},   // 2Y Note
        {102.0, 102.03125},   // 5Y Note
        {102.5, 102.53125},   // 10Y Note
        {103.0, 103.03125}    // 30Y Bond
    };
    
    // Place quotes on all instruments
    for (size_t i = 0; i < 6; ++i) {
        auto request = create_quote_request(instruments[i], prices[i].first, prices[i].second);
        bool success = quote_manager_->process_quote_update(request);
        EXPECT_TRUE(success) << "Failed to place quote for instrument " << i;
    }
    
    // Verify all quotes are active
    for (size_t i = 0; i < 6; ++i) {
        auto current_quote = quote_manager_->get_current_quote(instruments[i]);
        EXPECT_TRUE(current_quote.is_active) << "Quote not active for instrument " << i;
        EXPECT_DOUBLE_EQ(current_quote.bid_price.to_decimal(), prices[i].first);
        EXPECT_DOUBLE_EQ(current_quote.ask_price.to_decimal(), prices[i].second);
    }
    
    // Cancel all quotes
    quote_manager_->cancel_all_quotes();
    
    // Verify all quotes are cancelled
    for (size_t i = 0; i < 6; ++i) {
        auto current_quote = quote_manager_->get_current_quote(instruments[i]);
        EXPECT_FALSE(current_quote.is_active) << "Quote still active for instrument " << i;
    }
}

// Test rate limiting
TEST_F(TreasuryQuoteManagerTest, RateLimiting) {
    auto request = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125);
    
    // Process initial quote
    bool success = quote_manager_->process_quote_update(request);
    EXPECT_TRUE(success);
    
    // Immediately try to update again (should be rate limited)
    auto immediate_update = create_quote_request(TreasuryType::Note_10Y, 102.46875, 102.5);
    auto validation_result = quote_manager_->validate_quote(immediate_update);
    EXPECT_EQ(validation_result, QuoteValidationResult::RATE_LIMITED);
    
    // Wait and try again (simplified - in real test would need actual time delay)
    // For this test, we'll just verify the rate limiting mechanism works
}

// Test update threshold logic
TEST_F(TreasuryQuoteManagerTest, UpdateThresholds) {
    auto initial_request = create_quote_request(TreasuryType::Note_10Y, 102.5, 102.53125);
    bool success = quote_manager_->process_quote_update(initial_request);
    EXPECT_TRUE(success);
    
    // Small price change (should not warrant update)
    auto small_change = create_quote_request(TreasuryType::Note_10Y, 102.500001, 102.531251);
    auto validation_result = quote_manager_->validate_quote(small_change);
    EXPECT_EQ(validation_result, QuoteValidationResult::UPDATE_NOT_NEEDED);
    
    // Significant price change (should warrant update)
    auto significant_change = create_quote_request(TreasuryType::Note_10Y, 102.46875, 102.5);
    validation_result = quote_manager_->validate_quote(significant_change);
    EXPECT_EQ(validation_result, QuoteValidationResult::VALID);
}

// Test performance statistics
TEST_F(TreasuryQuoteManagerTest, PerformanceStatistics) {
    constexpr int num_operations = 100;
    
    for (int i = 0; i < num_operations; ++i) {
        double bid_price = 102.5 + (i * 0.03125); // Increment by 1/32nd each time
        double ask_price = bid_price + 0.03125;
        
        auto request = create_quote_request(TreasuryType::Note_10Y, bid_price, ask_price);
        
        if (i % 10 == 0) {
            // Introduce some validation failures
            auto invalid_request = create_quote_request(TreasuryType::Note_10Y, ask_price, bid_price);
            quote_manager_->validate_quote(invalid_request);
        } else {
            quote_manager_->process_quote_update(request);
        }
    }
    
    auto stats = quote_manager_->get_performance_stats();
    
    EXPECT_GT(stats.total_updates_processed, 0);
    EXPECT_GT(stats.validation_failures, 0);
    EXPECT_GT(stats.average_update_latency_ns, 0);
    EXPECT_LE(stats.average_update_latency_ns, 300); // Target performance
    EXPECT_LT(stats.update_success_rate, 1.0); // Should have some failures from invalid requests
    EXPECT_GT(stats.update_success_rate, 0.8); // But mostly successful
}

// Test edge cases and error handling
TEST_F(TreasuryQuoteManagerTest, EdgeCasesAndErrorHandling) {
    // Test with extreme prices
    auto extreme_high = create_quote_request(TreasuryType::Note_10Y, 999.0, 999.03125);
    auto result = quote_manager_->validate_quote(extreme_high);
    EXPECT_EQ(result, QuoteValidationResult::VALID); // Should handle extreme but valid prices
    
    auto extreme_low = create_quote_request(TreasuryType::Note_10Y, 0.03125, 0.0625);
    result = quote_manager_->validate_quote(extreme_low);
    EXPECT_EQ(result, QuoteValidationResult::VALID);
    
    // Test with zero prices (invalid)
    auto zero_bid = create_quote_request(TreasuryType::Note_10Y, 0.0, 0.03125);
    result = quote_manager_->validate_quote(zero_bid);
    // This should either be invalid price order or invalid spread
    EXPECT_NE(result, QuoteValidationResult::VALID);
}