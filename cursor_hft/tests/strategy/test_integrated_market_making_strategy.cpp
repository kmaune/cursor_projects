#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <random>
#include <vector>
#include <memory>
#include <cstring>

#include "hft/strategy/integrated_market_making_strategy.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::strategy;
using namespace hft::market_data;
using namespace hft::trading;

class IntegratedMarketMakingStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pools with sufficient capacity
        order_pool_ = std::make_unique<ObjectPool<TreasuryOrder, 1000>>();
        update_pool_ = std::make_unique<ObjectPool<MarketUpdate, 1000>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_);
        
        // Initialize strategy
        strategy_ = std::make_unique<IntegratedMarketMakingStrategy>(
            *order_pool_, *update_pool_, *order_book_);
    }

    void TearDown() override {
        strategy_.reset();
        order_book_.reset();
        update_pool_.reset();
        order_pool_.reset();
    }

    // Helper function to create market update
    static MarketUpdate create_market_update(TreasuryType instrument, 
                                    double bid_price, double ask_price,
                                    uint64_t bid_size = 1000000, uint64_t ask_size = 1000000) {
        return MarketUpdate(
            instrument,
            Price32nd::from_decimal(bid_price),
            Price32nd::from_decimal(ask_price),
            bid_size,
            ask_size
        );
    }

    std::unique_ptr<ObjectPool<TreasuryOrder, 1000>> order_pool_;
    std::unique_ptr<ObjectPool<MarketUpdate, 1000>> update_pool_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<IntegratedMarketMakingStrategy> strategy_;
};

// Test basic strategy decision making
TEST_F(IntegratedMarketMakingStrategyTest, BasicDecisionMaking) {
    auto market_update = create_market_update(TreasuryType::Note_10Y, 102.5, 102.6);
    
    auto decision = strategy_->make_integrated_decision(market_update);
    
    EXPECT_EQ(decision.instrument, TreasuryType::Note_10Y);
    EXPECT_NE(decision.action, TradingDecision::Action::NO_ACTION);
    EXPECT_GT(decision.decision_latency_ns, 0);
    EXPECT_LE(decision.decision_latency_ns, 1200); // 1.2μs target
    
    // Verify quote prices are reasonable
    if (decision.action == TradingDecision::Action::UPDATE_QUOTES) {
        EXPECT_LT(decision.bid_price.to_decimal(), decision.ask_price.to_decimal());
        EXPECT_GT(decision.bid_size, 100000); // Minimum $100k
        EXPECT_GT(decision.ask_size, 100000);
    }
}

// Test 32nds price alignment
TEST_F(IntegratedMarketMakingStrategyTest, TreasuryPricingCompliance) {
    auto market_update = create_market_update(TreasuryType::Note_10Y, 102.15625, 102.25); // 102 5/32, 102 8/32
    
    auto decision = strategy_->make_integrated_decision(market_update);
    
    if (decision.action == TradingDecision::Action::UPDATE_QUOTES) {
        // Verify prices are aligned to 32nds
        double bid_scaled = decision.bid_price.to_decimal() * 32.0;
        double ask_scaled = decision.ask_price.to_decimal() * 32.0;
        
        EXPECT_DOUBLE_EQ(bid_scaled, std::round(bid_scaled));
        EXPECT_DOUBLE_EQ(ask_scaled, std::round(ask_scaled));
    }
}

// Test performance under target latency
TEST_F(IntegratedMarketMakingStrategyTest, PerformanceTarget) {
    auto market_update = create_market_update(TreasuryType::Note_10Y, 102.5, 102.6);
    
    constexpr int num_iterations = 10000;
    std::vector<uint64_t> latencies;
    latencies.reserve(num_iterations);
    
    for (int i = 0; i < num_iterations; ++i) {
        auto decision = strategy_->make_integrated_decision(market_update);
        latencies.push_back(decision.decision_latency_ns);
    }
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    auto median = latencies[num_iterations / 2];
    auto p95 = latencies[static_cast<size_t>(num_iterations * 0.95)];
    auto p99 = latencies[static_cast<size_t>(num_iterations * 0.99)];
    
    // Performance assertions
    EXPECT_LE(median, 1200); // 1.2μs target
    EXPECT_LE(p95, 2000);    // 95th percentile under 2μs
    EXPECT_LE(p99, 5000);    // 99th percentile under 5μs
    
    std::cout << "Strategy Decision Latency Statistics:\n";
    std::cout << "  Median: " << median << "ns\n";
    std::cout << "  95th percentile: " << p95 << "ns\n";
    std::cout << "  99th percentile: " << p99 << "ns\n";
}

// Test position tracking and inventory management
TEST_F(IntegratedMarketMakingStrategyTest, PositionTracking) {
    // Simulate some position changes
    strategy_->update_position(TreasuryType::Note_10Y, 10000000, Price32nd::from_decimal(102.5));
    strategy_->update_position(TreasuryType::Note_5Y, -5000000, Price32nd::from_decimal(101.2));
    
    auto state = strategy_->get_current_state();
    
    EXPECT_EQ(state.positions[static_cast<size_t>(TreasuryType::Note_10Y)].load(), 10000000);
    EXPECT_EQ(state.positions[static_cast<size_t>(TreasuryType::Note_5Y)].load(), -5000000);
    EXPECT_GT(state.portfolio_dv01.load(), 0);
}

// Test risk limits enforcement
TEST_F(IntegratedMarketMakingStrategyTest, RiskLimitsEnforcement) {
    // Set large position to trigger risk limits
    strategy_->update_position(TreasuryType::Note_10Y, 95000000, Price32nd::from_decimal(102.5));
    
    auto market_update = create_market_update(TreasuryType::Note_10Y, 102.5, 102.6);
    auto decision = strategy_->make_integrated_decision(market_update);
    
    // Strategy should reduce quote sizes or cancel quotes due to position limits
    if (decision.action == TradingDecision::Action::UPDATE_QUOTES) {
        EXPECT_LT(decision.bid_size, 1000000); // Should be reduced from base size
        EXPECT_LT(decision.ask_size, 1000000);
    } else {
        EXPECT_EQ(decision.action, TradingDecision::Action::CANCEL_QUOTES);
    }
}

// Test yield curve integration
TEST_F(IntegratedMarketMakingStrategyTest, YieldCurveIntegration) {
    // Set up yield curve data
    std::array<double, 6> yields = {1.5, 2.0, 2.5, 3.0, 3.5, 4.0};
    std::array<double, 6> volatilities = {0.1, 0.15, 0.2, 0.25, 0.3, 0.35};
    
    strategy_->update_yield_curve_context(yields, volatilities);
    
    auto market_update = create_market_update(TreasuryType::Note_10Y, 102.5, 102.6);
    auto decision = strategy_->make_integrated_decision(market_update);
    
    // Verify yield curve data affects pricing (difficult to test precisely without internal access)
    EXPECT_NE(decision.action, TradingDecision::Action::NO_ACTION);
}

// Test adaptive complexity budget allocation
TEST_F(IntegratedMarketMakingStrategyTest, AdaptiveComplexity) {
    auto market_update = create_market_update(TreasuryType::Note_10Y, 102.5, 102.6);
    
    // Multiple calls to verify consistent performance
    constexpr int num_calls = 1000;
    std::vector<uint64_t> phase_latencies;
    
    for (int i = 0; i < num_calls; ++i) {
        auto decision = strategy_->make_integrated_decision(market_update);
        phase_latencies.push_back(decision.decision_latency_ns);
        
        // Verify adaptive behavior - later calls might be faster due to caching
        if (i > 100) {
            EXPECT_LE(decision.decision_latency_ns, 2000); // Should stabilize under 2μs
        }
    }
}

// Test multi-instrument decision making
TEST_F(IntegratedMarketMakingStrategyTest, MultiInstrumentSupport) {
    std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M, TreasuryType::Bill_6M, TreasuryType::Note_2Y,
        TreasuryType::Note_5Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y
    };
    
    std::array<std::pair<double, double>, 6> prices = {{
        {99.9, 99.95}, {99.8, 99.85}, {101.5, 101.6},
        {102.0, 102.1}, {102.5, 102.6}, {103.0, 103.1}
    }};
    
    for (size_t i = 0; i < 6; ++i) {
        auto market_update = create_market_update(instruments[i], prices[i].first, prices[i].second);
        auto decision = strategy_->make_integrated_decision(market_update);
        
        EXPECT_EQ(decision.instrument, instruments[i]);
        EXPECT_LE(decision.decision_latency_ns, 1200);
    }
}

// Test error handling and edge cases
TEST_F(IntegratedMarketMakingStrategyTest, EdgeCaseHandling) {
    // Test with very narrow spread
    auto narrow_market = create_market_update(TreasuryType::Note_10Y, 102.5, 102.50001);
    auto decision = strategy_->make_integrated_decision(narrow_market);
    EXPECT_NE(decision.action, TradingDecision::Action::UPDATE_QUOTES);
    
    // Test with zero sizes
    auto zero_size_market = create_market_update(TreasuryType::Note_10Y, 102.5, 102.6, 0, 0);
    decision = strategy_->make_integrated_decision(zero_size_market);
    EXPECT_LE(decision.decision_latency_ns, 1200);
    
    // Test with inverted market (bid > ask) - should not happen but test resilience
    auto inverted_market = create_market_update(TreasuryType::Note_10Y, 102.6, 102.5);
    decision = strategy_->make_integrated_decision(inverted_market);
    EXPECT_LE(decision.decision_latency_ns, 1200);
}

// Performance regression test
TEST_F(IntegratedMarketMakingStrategyTest, PerformanceRegression) {
    auto market_update = create_market_update(TreasuryType::Note_10Y, 102.5, 102.6);
    
    constexpr int warmup_iterations = 1000;
    constexpr int test_iterations = 10000;
    
    // Warmup
    for (int i = 0; i < warmup_iterations; ++i) {
        strategy_->make_integrated_decision(market_update);
    }
    
    // Actual test
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < test_iterations; ++i) {
        auto decision = strategy_->make_integrated_decision(market_update);
        EXPECT_LE(decision.decision_latency_ns, 1200); // Must meet target every time
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    auto avg_time_per_decision = total_time / test_iterations;
    
    std::cout << "Average time per decision: " << avg_time_per_decision << "ns\n";
    EXPECT_LE(avg_time_per_decision, 1200);
}