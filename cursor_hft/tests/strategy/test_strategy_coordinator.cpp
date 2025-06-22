#include <gtest/gtest.h>
#include <memory>
#include <array>
#include "hft/strategy/strategy_coordinator.hpp"
#include "hft/strategy/simple_market_maker.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::strategy;
using namespace hft::trading;
using namespace hft::market_data;

/**
 * @brief Test fixture for StrategyCoordinator
 * 
 * Sets up the infrastructure needed for multi-strategy coordination testing.
 * Tests both functional correctness and performance characteristics.
 */
class StrategyCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pools
        order_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrder, 4096>>();
        level_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>>();
        update_buffer_ = std::make_unique<hft::SPSCRingBuffer<OrderBookUpdate, 8192>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize strategy coordinator with multiple SimpleMarketMaker strategies
        coordinator_ = std::make_unique<StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>>(
            *order_pool_, *level_pool_, *update_buffer_, *order_book_
        );
    }

    void TearDown() override {
        coordinator_.reset();
        order_book_.reset();
        update_buffer_.reset();
        level_pool_.reset();
        order_pool_.reset();
    }

    // Helper function to create test market update
    SimpleMarketMaker::MarketUpdate create_test_market_update(
        TreasuryType instrument = TreasuryType::Note_10Y,
        double bid_price = 102.5,
        double ask_price = 102.53125,
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

protected:
    std::unique_ptr<hft::ObjectPool<TreasuryOrder, 4096>> order_pool_;
    std::unique_ptr<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>> level_pool_;
    std::unique_ptr<hft::SPSCRingBuffer<OrderBookUpdate, 8192>> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>> coordinator_;
};

// Test basic coordinator initialization
TEST_F(StrategyCoordinatorTest, BasicInitialization) {
    // Should initialize successfully
    EXPECT_NE(coordinator_, nullptr);
    
    // Should have zero coordination overhead initially
    EXPECT_EQ(coordinator_->get_average_coordination_latency_ns(), 0);
    
    // Should have default strategy configurations
    for (uint8_t i = 0; i < 3; ++i) {
        const auto& config = coordinator_->get_strategy_config(i);
        EXPECT_EQ(config.priority, i);  // Priority should match strategy index
        EXPECT_TRUE(config.enabled);
        EXPECT_EQ(config.resource_allocation, 1.0);
    }
    
    // Net positions should be zero initially
    for (auto instrument : {TreasuryType::Note_2Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y}) {
        const auto& net_pos = coordinator_->get_net_position(instrument);
        EXPECT_EQ(net_pos.total_position, 0);
        EXPECT_EQ(net_pos.total_unrealized_pnl, 0.0);
        EXPECT_EQ(net_pos.total_daily_pnl, 0.0);
        EXPECT_EQ(net_pos.active_strategies, 0);
    }
}

// Test strategy coordination functionality
TEST_F(StrategyCoordinatorTest, StrategyCoordination) {
    auto update = create_test_market_update();
    
    // Coordinate all strategies
    auto results = coordinator_->coordinate_strategies(update);
    
    // Should have results for all strategies
    EXPECT_EQ(results.size(), 3);
    
    // All strategies should generate quotes by default
    for (const auto& result : results) {
        EXPECT_EQ(result.action, StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyResult::Action::UPDATE_QUOTES);
        EXPECT_EQ(result.instrument, TreasuryType::Note_10Y);
        EXPECT_GT(result.bid_size, 0);
        EXPECT_GT(result.ask_size, 0);
        EXPECT_GT(result.execution_time_ns, 0);
        EXPECT_LT(result.coordination_overhead_ns, 1000);  // Should be well under 1μs
    }
    
    // Results should be sorted by priority (strategy 0 first, then 1, then 2)
    EXPECT_EQ(results[0].strategy_id, 0);
    EXPECT_EQ(results[1].strategy_id, 1);
    EXPECT_EQ(results[2].strategy_id, 2);
    
    EXPECT_EQ(results[0].priority, 0);
    EXPECT_EQ(results[1].priority, 1);
    EXPECT_EQ(results[2].priority, 2);
}

// Test strategy priority system
TEST_F(StrategyCoordinatorTest, StrategyPrioritySystem) {
    // Set different priorities (lower number = higher priority)
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig config0(2);  // Lowest priority
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig config1(0);  // Highest priority
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig config2(1);  // Medium priority
    
    EXPECT_TRUE(coordinator_->update_strategy_config(0, config0));
    EXPECT_TRUE(coordinator_->update_strategy_config(1, config1));
    EXPECT_TRUE(coordinator_->update_strategy_config(2, config2));
    
    auto update = create_test_market_update();
    auto results = coordinator_->coordinate_strategies(update);
    
    // Results should be sorted by priority: strategy 1 (priority 0), strategy 2 (priority 1), strategy 0 (priority 2)
    EXPECT_EQ(results[0].strategy_id, 1);
    EXPECT_EQ(results[0].priority, 0);
    
    EXPECT_EQ(results[1].strategy_id, 2);
    EXPECT_EQ(results[1].priority, 1);
    
    EXPECT_EQ(results[2].strategy_id, 0);
    EXPECT_EQ(results[2].priority, 2);
}

// Test resource allocation
TEST_F(StrategyCoordinatorTest, ResourceAllocation) {
    // Set different resource allocations
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig config0(0, true, 1.0);   // Full allocation
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig config1(1, true, 0.5);   // Half allocation
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig config2(2, true, 0.25);  // Quarter allocation
    
    EXPECT_TRUE(coordinator_->update_strategy_config(0, config0));
    EXPECT_TRUE(coordinator_->update_strategy_config(1, config1));
    EXPECT_TRUE(coordinator_->update_strategy_config(2, config2));
    
    auto update = create_test_market_update();
    auto results = coordinator_->coordinate_strategies(update);
    
    // Base quote size should be 1M, so allocations should be proportional
    const uint64_t base_size = 1000000;
    
    EXPECT_EQ(results[0].bid_size, base_size);           // 100% allocation
    EXPECT_EQ(results[1].bid_size, base_size / 2);       // 50% allocation  
    EXPECT_EQ(results[2].bid_size, base_size / 4);       // 25% allocation
    
    EXPECT_EQ(results[0].ask_size, base_size);
    EXPECT_EQ(results[1].ask_size, base_size / 2);
    EXPECT_EQ(results[2].ask_size, base_size / 4);
}

// Test strategy enable/disable
TEST_F(StrategyCoordinatorTest, StrategyEnableDisable) {
    // Disable strategy 1
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig config1(1, false);  // Disabled
    EXPECT_TRUE(coordinator_->update_strategy_config(1, config1));
    
    auto update = create_test_market_update();
    auto results = coordinator_->coordinate_strategies(update);
    
    // Strategy 0 and 2 should generate quotes
    EXPECT_EQ(results[0].action, StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyResult::Action::UPDATE_QUOTES);
    EXPECT_EQ(results[2].action, StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyResult::Action::UPDATE_QUOTES);
    
    // Strategy 1 should be disabled (NO_ACTION)
    EXPECT_EQ(results[1].action, StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyResult::Action::NO_ACTION);
    EXPECT_EQ(results[1].strategy_id, 1);
}

// Test position netting across strategies
TEST_F(StrategyCoordinatorTest, PositionNetting) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Initial net position should be zero
    auto initial_net_pos = coordinator_->get_net_position(instrument);
    EXPECT_EQ(initial_net_pos.total_position, 0);
    EXPECT_EQ(initial_net_pos.active_strategies, 0);
    
    // Manually add positions to strategies (simulating fills)
    // Note: We'd need access to individual strategies to do this properly
    // For now, we'll test the coordination and netting framework
    
    auto update = create_test_market_update(instrument);
    auto results = coordinator_->coordinate_strategies(update);
    
    // Position netting should be updated after coordination
    coordinator_->update_position_netting(instrument);
    
    auto updated_net_pos = coordinator_->get_net_position(instrument);
    EXPECT_GE(updated_net_pos.last_update_time_ns, initial_net_pos.last_update_time_ns);
}

// Test emergency stop functionality
TEST_F(StrategyCoordinatorTest, EmergencyStop) {
    // All strategies should be enabled initially
    for (uint8_t i = 0; i < 3; ++i) {
        EXPECT_TRUE(coordinator_->get_strategy_config(i).enabled);
    }
    
    // Trigger emergency stop
    coordinator_->emergency_stop();
    
    // All strategies should be disabled after emergency stop
    for (uint8_t i = 0; i < 3; ++i) {
        EXPECT_FALSE(coordinator_->get_strategy_config(i).enabled);
    }
    
    // Coordination should return NO_ACTION for all strategies
    auto update = create_test_market_update();
    auto results = coordinator_->coordinate_strategies(update);
    
    for (const auto& result : results) {
        EXPECT_EQ(result.action, StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyResult::Action::NO_ACTION);
    }
}

// Test coordination performance
TEST_F(StrategyCoordinatorTest, CoordinationPerformance) {
    auto update = create_test_market_update();
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        coordinator_->coordinate_strategies(update);
    }
    
    // Measure coordination latency
    const int iterations = 1000;
    uint64_t total_coordination_time = 0;
    uint64_t max_coordination_time = 0;
    uint64_t min_coordination_time = UINT64_MAX;
    
    for (int i = 0; i < iterations; ++i) {
        const auto start_time = hft::HFTTimer::get_timestamp_ns();
        auto results = coordinator_->coordinate_strategies(update);
        const auto end_time = hft::HFTTimer::get_timestamp_ns();
        
        const uint64_t coordination_time = end_time - start_time;
        total_coordination_time += coordination_time;
        max_coordination_time = std::max(max_coordination_time, coordination_time);
        min_coordination_time = std::min(min_coordination_time, coordination_time);
        
        // Each result should have reasonable coordination overhead
        for (const auto& result : results) {
            EXPECT_LT(result.coordination_overhead_ns, 1000);  // Less than 1μs overhead
        }
    }
    
    const uint64_t avg_coordination_time = total_coordination_time / iterations;
    
    // Performance validation
    EXPECT_LT(avg_coordination_time, 500);   // Average < 500ns
    EXPECT_LT(max_coordination_time, 2000);  // Max < 2μs
    EXPECT_GT(min_coordination_time, 0);     // Should measure some time
    
    // Coordinator should track average latency
    EXPECT_GT(coordinator_->get_average_coordination_latency_ns(), 0);
    EXPECT_LT(coordinator_->get_average_coordination_latency_ns(), 1000);  // Should be well under 1μs
    
    std::cout << "Coordination Performance:" << std::endl;
    std::cout << "  Average: " << avg_coordination_time << "ns" << std::endl;
    std::cout << "  Min: " << min_coordination_time << "ns" << std::endl;
    std::cout << "  Max: " << max_coordination_time << "ns" << std::endl;
    std::cout << "  Target: <100ns" << std::endl;
}

// Test multi-instrument coordination
TEST_F(StrategyCoordinatorTest, MultiInstrumentCoordination) {
    const std::array<TreasuryType, 3> instruments = {
        TreasuryType::Note_2Y,
        TreasuryType::Note_10Y,
        TreasuryType::Bond_30Y
    };
    
    // Test coordination for each instrument
    for (auto instrument : instruments) {
        auto update = create_test_market_update(instrument);
        auto results = coordinator_->coordinate_strategies(update);
        
        // All strategies should handle all instruments
        for (const auto& result : results) {
            if (result.action == StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyResult::Action::UPDATE_QUOTES) {
                EXPECT_EQ(result.instrument, instrument);
                EXPECT_GT(result.bid_size, 0);
                EXPECT_GT(result.ask_size, 0);
            }
        }
        
        // Update position netting for each instrument
        coordinator_->update_position_netting(instrument);
        
        const auto& net_pos = coordinator_->get_net_position(instrument);
        EXPECT_GT(net_pos.last_update_time_ns, 0);
    }
}

// Test configuration validation
TEST_F(StrategyCoordinatorTest, ConfigurationValidation) {
    // Valid configuration updates should succeed
    StrategyCoordinator<SimpleMarketMaker, SimpleMarketMaker, SimpleMarketMaker>::StrategyConfig valid_config(1, true, 0.8);
    EXPECT_TRUE(coordinator_->update_strategy_config(0, valid_config));
    EXPECT_TRUE(coordinator_->update_strategy_config(1, valid_config));
    EXPECT_TRUE(coordinator_->update_strategy_config(2, valid_config));
    
    // Invalid strategy ID should fail
    EXPECT_FALSE(coordinator_->update_strategy_config(10, valid_config));
    
    // Configuration should be applied correctly
    const auto& applied_config = coordinator_->get_strategy_config(0);
    EXPECT_EQ(applied_config.priority, 1);
    EXPECT_TRUE(applied_config.enabled);
    EXPECT_EQ(applied_config.resource_allocation, 0.8);
}

// Test consistency across multiple coordination cycles
TEST_F(StrategyCoordinatorTest, ConsistencyAcrossMultipleCycles) {
    auto update = create_test_market_update();
    
    // Run multiple coordination cycles
    const int cycles = 100;
    std::array<decltype(coordinator_->coordinate_strategies(update)), cycles> all_results;
    
    for (int i = 0; i < cycles; ++i) {
        all_results[i] = coordinator_->coordinate_strategies(update);
    }
    
    // Results should be consistent across cycles (same input -> same output)
    for (int i = 1; i < cycles; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            const auto& first = all_results[0][j];
            const auto& current = all_results[i][j];
            
            EXPECT_EQ(first.action, current.action);
            EXPECT_EQ(first.strategy_id, current.strategy_id);
            EXPECT_EQ(first.priority, current.priority);
            EXPECT_EQ(first.instrument, current.instrument);
            EXPECT_EQ(first.bid_size, current.bid_size);
            EXPECT_EQ(first.ask_size, current.ask_size);
            
            // Timing should be measured but may vary
            EXPECT_GT(current.execution_time_ns, 0);
            EXPECT_GT(current.coordination_overhead_ns, 0);
        }
    }
}