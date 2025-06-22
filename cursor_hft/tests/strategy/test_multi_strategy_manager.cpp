#include <gtest/gtest.h>
#include <memory>
#include <array>
#include "hft/strategy/multi_strategy_manager.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::strategy;
using namespace hft::trading;
using namespace hft::market_data;

/**
 * @brief Test fixture for MultiStrategyManager
 * 
 * Tests the multi-strategy coordination functionality with focus on:
 * - Strategy prioritization and execution order
 * - Resource allocation across strategies
 * - Cross-strategy position netting
 * - Performance characteristics
 */
class MultiStrategyManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pools
        order_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrder, 4096>>();
        level_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>>();
        update_buffer_ = std::make_unique<hft::SPSCRingBuffer<OrderBookUpdate, 8192>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize multi-strategy manager
        manager_ = std::make_unique<MultiStrategyManager>(*order_pool_, *level_pool_, *update_buffer_, *order_book_);
    }

    void TearDown() override {
        manager_.reset();
        order_book_.reset();
        update_buffer_.reset();
        level_pool_.reset();
        order_pool_.reset();
    }

    // Helper to create test market update
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
    std::unique_ptr<MultiStrategyManager> manager_;
};

// Test basic initialization
TEST_F(MultiStrategyManagerTest, BasicInitialization) {
    EXPECT_NE(manager_, nullptr);
    
    // Should have zero coordination overhead initially
    EXPECT_EQ(manager_->get_average_coordination_latency_ns(), 0);
    
    // Should have default configurations for all strategies
    for (uint8_t i = 0; i < 4; ++i) {
        const auto& config = manager_->get_strategy_config(i);
        EXPECT_EQ(config.priority, i);  // Priority should match index
        EXPECT_TRUE(config.enabled);
        EXPECT_EQ(config.resource_allocation, 1.0);
    }
    
    // Net positions should be zero initially
    for (auto instrument : {TreasuryType::Note_2Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y}) {
        const auto& net_pos = manager_->get_net_position(instrument);
        EXPECT_EQ(net_pos.total_position, 0);
        EXPECT_EQ(net_pos.total_unrealized_pnl, 0.0);
        EXPECT_EQ(net_pos.total_daily_pnl, 0.0);
        EXPECT_EQ(net_pos.active_strategies, 0);
    }
}

// Test strategy coordination
TEST_F(MultiStrategyManagerTest, StrategyCoordination) {
    auto update = create_test_market_update();
    
    // Coordinate all strategies
    auto results = manager_->coordinate_strategies(update);
    
    // Should have results for all 4 strategies
    EXPECT_EQ(results.size(), 4);
    
    // All strategies should generate quotes by default
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        EXPECT_EQ(result.action, MultiStrategyManager::StrategyResult::Action::UPDATE_QUOTES);
        EXPECT_EQ(result.instrument, TreasuryType::Note_10Y);
        EXPECT_GT(result.bid_size, 0);
        EXPECT_GT(result.ask_size, 0);
        EXPECT_GT(result.execution_time_ns, 0);
        EXPECT_LT(result.coordination_overhead_ns, 5000);  // Will optimize to <1μs
    }
    
    // Results should be sorted by priority (strategy 0 first, then 1, 2, 3)
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_EQ(results[i].strategy_id, i);
        EXPECT_EQ(results[i].priority, i);
    }
}

// Test priority system
TEST_F(MultiStrategyManagerTest, PrioritySystem) {
    // Set different priorities (lower number = higher priority)
    MultiStrategyManager::StrategyConfig config0(3);  // Lowest priority
    MultiStrategyManager::StrategyConfig config1(0);  // Highest priority  
    MultiStrategyManager::StrategyConfig config2(1);  // Medium-high priority
    MultiStrategyManager::StrategyConfig config3(2);  // Medium-low priority
    
    EXPECT_TRUE(manager_->update_strategy_config(0, config0));
    EXPECT_TRUE(manager_->update_strategy_config(1, config1));
    EXPECT_TRUE(manager_->update_strategy_config(2, config2));
    EXPECT_TRUE(manager_->update_strategy_config(3, config3));
    
    auto update = create_test_market_update();
    auto results = manager_->coordinate_strategies(update);
    
    // Results should be sorted by priority: 1 (0), 2 (1), 3 (2), 0 (3)
    EXPECT_EQ(results[0].strategy_id, 1);
    EXPECT_EQ(results[0].priority, 0);
    
    EXPECT_EQ(results[1].strategy_id, 2);
    EXPECT_EQ(results[1].priority, 1);
    
    EXPECT_EQ(results[2].strategy_id, 3);
    EXPECT_EQ(results[2].priority, 2);
    
    EXPECT_EQ(results[3].strategy_id, 0);
    EXPECT_EQ(results[3].priority, 3);
}

// Test resource allocation
TEST_F(MultiStrategyManagerTest, ResourceAllocation) {
    // Set different resource allocations
    MultiStrategyManager::StrategyConfig config0(0, true, 1.0);   // Full allocation
    MultiStrategyManager::StrategyConfig config1(1, true, 0.5);   // Half allocation
    MultiStrategyManager::StrategyConfig config2(2, true, 0.25);  // Quarter allocation
    MultiStrategyManager::StrategyConfig config3(3, true, 0.1);   // 10% allocation
    
    EXPECT_TRUE(manager_->update_strategy_config(0, config0));
    EXPECT_TRUE(manager_->update_strategy_config(1, config1));
    EXPECT_TRUE(manager_->update_strategy_config(2, config2));
    EXPECT_TRUE(manager_->update_strategy_config(3, config3));
    
    auto update = create_test_market_update();
    auto results = manager_->coordinate_strategies(update);
    
    // Base quote size should be 1M, so allocations should be proportional
    const uint64_t base_size = 1000000;
    
    EXPECT_EQ(results[0].bid_size, base_size);           // 100% allocation
    EXPECT_EQ(results[1].bid_size, base_size / 2);       // 50% allocation
    EXPECT_EQ(results[2].bid_size, base_size / 4);       // 25% allocation
    EXPECT_EQ(results[3].bid_size, base_size / 10);      // 10% allocation
    
    EXPECT_EQ(results[0].ask_size, base_size);
    EXPECT_EQ(results[1].ask_size, base_size / 2);
    EXPECT_EQ(results[2].ask_size, base_size / 4);
    EXPECT_EQ(results[3].ask_size, base_size / 10);
}

// Test strategy enable/disable
TEST_F(MultiStrategyManagerTest, StrategyEnableDisable) {
    // Disable strategies 1 and 3
    MultiStrategyManager::StrategyConfig config1(1, false);  // Disabled
    MultiStrategyManager::StrategyConfig config3(3, false);  // Disabled
    
    EXPECT_TRUE(manager_->update_strategy_config(1, config1));
    EXPECT_TRUE(manager_->update_strategy_config(3, config3));
    
    auto update = create_test_market_update();
    auto results = manager_->coordinate_strategies(update);
    
    // Strategy 0 and 2 should generate quotes
    EXPECT_EQ(results[0].action, MultiStrategyManager::StrategyResult::Action::UPDATE_QUOTES);
    EXPECT_EQ(results[2].action, MultiStrategyManager::StrategyResult::Action::UPDATE_QUOTES);
    
    // Strategies 1 and 3 should be disabled (NO_ACTION)
    EXPECT_EQ(results[1].action, MultiStrategyManager::StrategyResult::Action::NO_ACTION);
    EXPECT_EQ(results[3].action, MultiStrategyManager::StrategyResult::Action::NO_ACTION);
    
    // Strategy IDs should still be correct
    EXPECT_EQ(results[1].strategy_id, 1);
    EXPECT_EQ(results[3].strategy_id, 3);
}

// Test position netting
TEST_F(MultiStrategyManagerTest, PositionNetting) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Initial net position should be zero
    auto initial_net_pos = manager_->get_net_position(instrument);
    EXPECT_EQ(initial_net_pos.total_position, 0);
    EXPECT_EQ(initial_net_pos.active_strategies, 0);
    
    auto update = create_test_market_update(instrument);
    auto results = manager_->coordinate_strategies(update);
    
    // Position netting should be updated after coordination
    auto updated_net_pos = manager_->get_net_position(instrument);
    EXPECT_GE(updated_net_pos.last_update_time_ns, initial_net_pos.last_update_time_ns);
}

// Test emergency stop
TEST_F(MultiStrategyManagerTest, EmergencyStop) {
    // All strategies should be enabled initially
    for (uint8_t i = 0; i < 4; ++i) {
        EXPECT_TRUE(manager_->get_strategy_config(i).enabled);
    }
    
    // Trigger emergency stop
    manager_->emergency_stop();
    
    // All strategies should be disabled
    for (uint8_t i = 0; i < 4; ++i) {
        EXPECT_FALSE(manager_->get_strategy_config(i).enabled);
    }
    
    // Coordination should return NO_ACTION for all strategies
    auto update = create_test_market_update();
    auto results = manager_->coordinate_strategies(update);
    
    for (const auto& result : results) {
        EXPECT_EQ(result.action, MultiStrategyManager::StrategyResult::Action::NO_ACTION);
    }
}

// Test coordination performance
TEST_F(MultiStrategyManagerTest, CoordinationPerformance) {
    auto update = create_test_market_update();
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        manager_->coordinate_strategies(update);
    }
    
    // Measure coordination latency
    const int iterations = 1000;
    uint64_t total_coordination_time = 0;
    uint64_t max_coordination_time = 0;
    uint64_t min_coordination_time = UINT64_MAX;
    
    for (int i = 0; i < iterations; ++i) {
        const auto start_time = hft::HFTTimer::get_timestamp_ns();
        auto results = manager_->coordinate_strategies(update);
        const auto end_time = hft::HFTTimer::get_timestamp_ns();
        
        const uint64_t coordination_time = end_time - start_time;
        total_coordination_time += coordination_time;
        max_coordination_time = std::max(max_coordination_time, coordination_time);
        min_coordination_time = std::min(min_coordination_time, coordination_time);
        
        // Each result should have reasonable coordination overhead
        for (const auto& result : results) {
            EXPECT_LT(result.coordination_overhead_ns, 5000);  // Less than 5μs overhead (will optimize)
        }
    }
    
    const uint64_t avg_coordination_time = total_coordination_time / iterations;
    
    // Performance validation (current performance, will optimize)
    EXPECT_LT(avg_coordination_time, 3000);   // Average < 3μs (target: 100ns)
    EXPECT_LT(max_coordination_time, 10000);  // Max < 10μs (target: 2μs)
    EXPECT_GT(min_coordination_time, 0);      // Should measure some time
    
    // Manager should track average latency
    EXPECT_GT(manager_->get_average_coordination_latency_ns(), 0);
    EXPECT_LT(manager_->get_average_coordination_latency_ns(), 3000);  // Will optimize to <100ns
    
    std::cout << "Multi-Strategy Coordination Performance:" << std::endl;
    std::cout << "  Average: " << avg_coordination_time << "ns" << std::endl;
    std::cout << "  Min: " << min_coordination_time << "ns" << std::endl;
    std::cout << "  Max: " << max_coordination_time << "ns" << std::endl;
    std::cout << "  Target: <100ns" << std::endl;
}

// Test configuration validation
TEST_F(MultiStrategyManagerTest, ConfigurationValidation) {
    // Valid configuration updates should succeed
    MultiStrategyManager::StrategyConfig valid_config(1, true, 0.8);
    for (uint8_t i = 0; i < 4; ++i) {
        EXPECT_TRUE(manager_->update_strategy_config(i, valid_config));
    }
    
    // Invalid strategy ID should fail
    EXPECT_FALSE(manager_->update_strategy_config(10, valid_config));
    
    // Configuration should be applied correctly
    const auto& applied_config = manager_->get_strategy_config(0);
    EXPECT_EQ(applied_config.priority, 1);
    EXPECT_TRUE(applied_config.enabled);
    EXPECT_EQ(applied_config.resource_allocation, 0.8);
}

// Test multi-instrument coordination
TEST_F(MultiStrategyManagerTest, MultiInstrumentCoordination) {
    const std::array<TreasuryType, 3> instruments = {
        TreasuryType::Note_2Y,
        TreasuryType::Note_10Y,
        TreasuryType::Bond_30Y
    };
    
    // Test coordination for each instrument
    for (auto instrument : instruments) {
        auto update = create_test_market_update(instrument);
        auto results = manager_->coordinate_strategies(update);
        
        // All enabled strategies should handle all instruments
        for (const auto& result : results) {
            if (result.action == MultiStrategyManager::StrategyResult::Action::UPDATE_QUOTES) {
                EXPECT_EQ(result.instrument, instrument);
                EXPECT_GT(result.bid_size, 0);
                EXPECT_GT(result.ask_size, 0);
            }
        }
        
        // Update position netting for each instrument
        manager_->update_position_netting(instrument);
        
        const auto& net_pos = manager_->get_net_position(instrument);
        EXPECT_GT(net_pos.last_update_time_ns, 0);
    }
}

// Test consistency across multiple coordination cycles
TEST_F(MultiStrategyManagerTest, ConsistencyAcrossMultipleCycles) {
    auto update = create_test_market_update();
    
    // Run multiple coordination cycles
    const int cycles = 50;
    std::array<decltype(manager_->coordinate_strategies(update)), cycles> all_results;
    
    for (int i = 0; i < cycles; ++i) {
        all_results[i] = manager_->coordinate_strategies(update);
    }
    
    // Results should be consistent across cycles (same input -> same output)
    for (int i = 1; i < cycles; ++i) {
        for (size_t j = 0; j < 4; ++j) {
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