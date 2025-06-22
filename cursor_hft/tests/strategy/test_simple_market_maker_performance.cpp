#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <numeric>
#include "hft/strategy/simple_market_maker.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::strategy;
using namespace hft::trading;
using namespace hft::market_data;

/**
 * @brief Performance-focused tests for SimpleMarketMaker
 * 
 * These tests focus on latency measurements and performance characteristics.
 * They are separated from functional tests to avoid flakiness in CI environments.
 * 
 * Note: Performance tests may be affected by system load and should be run
 * in controlled environments for accurate measurements.
 */
class SimpleMarketMakerPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pools
        order_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrder, 4096>>();
        level_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>>();
        update_buffer_ = std::make_unique<hft::SPSCRingBuffer<OrderBookUpdate, 8192>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize strategy
        strategy_ = std::make_unique<SimpleMarketMaker>(*order_pool_, *order_book_);
    }

    void TearDown() override {
        strategy_.reset();
        order_book_.reset();
        update_buffer_.reset();
        level_pool_.reset();
        order_pool_.reset();
    }

    // Helper function to create test market update
    SimpleMarketMaker::MarketUpdate create_test_market_update(
        TreasuryType instrument = TreasuryType::Note_10Y,
        double bid_price = 102.5,
        double ask_price = 102.53125,  // 102 17/32
        uint64_t bid_size = 5000000,
        uint64_t ask_size = 5000000
    ) {
        SimpleMarketMaker::MarketUpdate update;
        update.instrument = instrument;
        update.best_bid = Price32nd::from_decimal(bid_price);
        update.best_ask = Price32nd::from_decimal(ask_price);
        update.bid_size = bid_size;
        update.ask_size = ask_size;
        update.update_time_ns = hft::HFTTimer::get_timestamp_ns();
        return update;
    }

    // Statistical analysis helper
    struct LatencyStats {
        uint64_t min_ns;
        uint64_t max_ns;
        uint64_t median_ns;
        uint64_t p95_ns;
        uint64_t p99_ns;
        double mean_ns;
        double stddev_ns;
    };

    LatencyStats analyze_latencies(const std::vector<uint64_t>& latencies) {
        auto sorted_latencies = latencies;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        
        const size_t size = sorted_latencies.size();
        const double mean = std::accumulate(latencies.begin(), latencies.end(), 0.0) / size;
        
        double variance = 0.0;
        for (const auto& latency : latencies) {
            variance += (latency - mean) * (latency - mean);
        }
        variance /= size;
        
        return LatencyStats{
            .min_ns = sorted_latencies[0],
            .max_ns = sorted_latencies[size - 1],
            .median_ns = sorted_latencies[size / 2],
            .p95_ns = sorted_latencies[static_cast<size_t>(size * 0.95)],
            .p99_ns = sorted_latencies[static_cast<size_t>(size * 0.99)],
            .mean_ns = mean,
            .stddev_ns = std::sqrt(variance)
        };
    }

protected:
    std::unique_ptr<hft::ObjectPool<TreasuryOrder, 4096>> order_pool_;
    std::unique_ptr<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>> level_pool_;
    std::unique_ptr<hft::SPSCRingBuffer<OrderBookUpdate, 8192>> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<SimpleMarketMaker> strategy_;
};

// Test single decision latency target
TEST_F(SimpleMarketMakerPerformanceTest, SingleDecisionLatency) {
    auto update = create_test_market_update();
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        strategy_->make_decision(update);
    }
    
    auto decision = strategy_->make_decision(update);
    
    // Basic functionality should work
    EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    
    // Performance target: <2μs (2000ns) with some margin for CI environments
    EXPECT_LE(decision.decision_latency_ns, 2000) 
        << "Decision latency " << decision.decision_latency_ns << "ns exceeds 2μs target";
    
    // Should measure time
    EXPECT_GT(decision.decision_latency_ns, 0);
    
    // Report the actual latency for informational purposes
    std::cout << "Single decision latency: " << decision.decision_latency_ns << "ns" << std::endl;
}

// Test latency distribution over many decisions
TEST_F(SimpleMarketMakerPerformanceTest, LatencyDistribution) {
    auto update = create_test_market_update();
    
    const int num_iterations = 10000;
    std::vector<uint64_t> latencies;
    latencies.reserve(num_iterations);
    
    // Warm up
    for (int i = 0; i < 100; ++i) {
        strategy_->make_decision(update);
    }
    
    // Collect latency measurements
    for (int i = 0; i < num_iterations; ++i) {
        auto decision = strategy_->make_decision(update);
        latencies.push_back(decision.decision_latency_ns);
        
        // All decisions should be functional
        EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    }
    
    // Analyze latency distribution
    auto stats = analyze_latencies(latencies);
    
    // Report statistics
    std::cout << "Latency Statistics (ns):" << std::endl;
    std::cout << "  Min: " << stats.min_ns << std::endl;
    std::cout << "  Max: " << stats.max_ns << std::endl;
    std::cout << "  Mean: " << stats.mean_ns << std::endl;
    std::cout << "  Median: " << stats.median_ns << std::endl;
    std::cout << "  95th percentile: " << stats.p95_ns << std::endl;
    std::cout << "  99th percentile: " << stats.p99_ns << std::endl;
    std::cout << "  Std dev: " << stats.stddev_ns << std::endl;
    
    // Reasonable performance expectations (looser for CI)
    EXPECT_LE(stats.p95_ns, 3000) << "95th percentile latency too high";
    EXPECT_LE(stats.p99_ns, 5000) << "99th percentile latency too high";
    EXPECT_GT(stats.min_ns, 0) << "Minimum latency should be positive";
    
    // Consistency check (reasonable variance)
    EXPECT_LE(stats.max_ns, stats.mean_ns * 10) << "Latency variance too high";
}

// Test performance under different market conditions
TEST_F(SimpleMarketMakerPerformanceTest, PerformanceWithVaryingConditions) {
    const int iterations_per_condition = 1000;
    std::vector<uint64_t> all_latencies;
    
    // Test different market conditions
    const std::vector<std::pair<double, double>> market_conditions = {
        {100.0, 100.03125},   // Tight spread
        {100.0, 100.25},      // Wide spread  
        {99.0, 101.0},        // Very wide spread
        {102.0, 102.0625},    // Normal treasury spread
    };
    
    for (const auto& [bid, ask] : market_conditions) {
        auto update = create_test_market_update(TreasuryType::Note_10Y, bid, ask);
        
        for (int i = 0; i < iterations_per_condition; ++i) {
            auto decision = strategy_->make_decision(update);
            all_latencies.push_back(decision.decision_latency_ns);
            
            // Should handle all conditions
            EXPECT_NE(decision.action, SimpleMarketMaker::TradingDecision::Action::NO_ACTION);
        }
    }
    
    auto stats = analyze_latencies(all_latencies);
    
    std::cout << "Performance across varying conditions:" << std::endl;
    std::cout << "  Mean latency: " << stats.mean_ns << "ns" << std::endl;
    std::cout << "  95th percentile: " << stats.p95_ns << "ns" << std::endl;
    
    // Should maintain reasonable performance across all conditions
    EXPECT_LE(stats.p95_ns, 3000) << "Performance degraded under varying conditions";
}

// Test performance with position changes
TEST_F(SimpleMarketMakerPerformanceTest, PerformanceWithPositionUpdates) {
    auto update = create_test_market_update();
    
    const int num_iterations = 1000;
    std::vector<uint64_t> latencies;
    latencies.reserve(num_iterations);
    
    int64_t position_change = 1000000;  // $1M
    
    for (int i = 0; i < num_iterations; ++i) {
        auto decision = strategy_->make_decision(update);
        latencies.push_back(decision.decision_latency_ns);
        
        // Update position to test inventory penalty calculation
        strategy_->update_position(TreasuryType::Note_10Y, position_change, 
                                 Price32nd::from_decimal(102.5));
        position_change = -position_change;  // Alternate direction
    }
    
    auto stats = analyze_latencies(latencies);
    
    std::cout << "Performance with position updates:" << std::endl;
    std::cout << "  Mean latency: " << stats.mean_ns << "ns" << std::endl;
    std::cout << "  95th percentile: " << stats.p95_ns << "ns" << std::endl;
    
    // Position updates shouldn't significantly impact performance
    EXPECT_LE(stats.p95_ns, 4000) << "Position updates caused performance regression";
}

// Test multi-instrument performance
TEST_F(SimpleMarketMakerPerformanceTest, MultiInstrumentPerformance) {
    const std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M,
        TreasuryType::Bill_6M,
        TreasuryType::Note_2Y,
        TreasuryType::Note_5Y,
        TreasuryType::Note_10Y,
        TreasuryType::Bond_30Y
    };
    
    const int iterations_per_instrument = 500;
    std::vector<uint64_t> all_latencies;
    
    for (const auto instrument : instruments) {
        auto update = create_test_market_update(instrument);
        
        for (int i = 0; i < iterations_per_instrument; ++i) {
            auto decision = strategy_->make_decision(update);
            all_latencies.push_back(decision.decision_latency_ns);
            
            EXPECT_EQ(decision.instrument, instrument);
        }
    }
    
    auto stats = analyze_latencies(all_latencies);
    
    std::cout << "Multi-instrument performance:" << std::endl;
    std::cout << "  Mean latency: " << stats.mean_ns << "ns" << std::endl;
    std::cout << "  95th percentile: " << stats.p95_ns << "ns" << std::endl;
    
    // Performance should be consistent across instruments
    EXPECT_LE(stats.p95_ns, 3000) << "Multi-instrument performance degraded";
}

// Test performance regression detection
TEST_F(SimpleMarketMakerPerformanceTest, PerformanceRegression) {
    auto update = create_test_market_update();
    
    const int warm_up_iterations = 1000;
    const int measurement_iterations = 5000;
    
    // Warm up
    for (int i = 0; i < warm_up_iterations; ++i) {
        strategy_->make_decision(update);
    }
    
    // Measure baseline performance
    std::vector<uint64_t> baseline_latencies;
    baseline_latencies.reserve(measurement_iterations);
    
    for (int i = 0; i < measurement_iterations; ++i) {
        auto decision = strategy_->make_decision(update);
        baseline_latencies.push_back(decision.decision_latency_ns);
    }
    
    auto baseline_stats = analyze_latencies(baseline_latencies);
    
    // Report baseline for comparison in future runs
    std::cout << "Baseline performance metrics:" << std::endl;
    std::cout << "  Mean: " << baseline_stats.mean_ns << "ns" << std::endl;
    std::cout << "  Median: " << baseline_stats.median_ns << "ns" << std::endl;
    std::cout << "  95th percentile: " << baseline_stats.p95_ns << "ns" << std::endl;
    std::cout << "  99th percentile: " << baseline_stats.p99_ns << "ns" << std::endl;
    
    // Conservative performance targets for regression detection
    EXPECT_LE(baseline_stats.median_ns, 1000) << "Median latency regression detected";
    EXPECT_LE(baseline_stats.p95_ns, 3000) << "95th percentile latency regression detected";
    EXPECT_LE(baseline_stats.p99_ns, 10000) << "99th percentile latency regression detected";
}