#include <gtest/gtest.h>
#include <vector>
#include <array>

#include "hft/strategy/inventory_manager.hpp"

using namespace hft::strategy;
using namespace hft::market_data;

class InventoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        inventory_manager_ = std::make_unique<InventoryManager>();
    }

    void TearDown() override {
        inventory_manager_.reset();
    }

    TradeExecution create_trade(TreasuryType instrument, int64_t quantity, double price) {
        return TradeExecution(instrument, quantity, Price32nd::from_decimal(price));
    }

    std::unique_ptr<InventoryManager> inventory_manager_;
};

// Test basic position tracking
TEST_F(InventoryManagerTest, BasicPositionTracking) {
    // Initial position should be zero
    EXPECT_EQ(inventory_manager_->get_position(TreasuryType::Note_10Y), 0);
    
    // Add a long position
    auto trade = create_trade(TreasuryType::Note_10Y, 10000000, 102.5); // $10M long
    bool success = inventory_manager_->update_position(trade);
    EXPECT_TRUE(success);
    
    EXPECT_EQ(inventory_manager_->get_position(TreasuryType::Note_10Y), 10000000);
    
    // Add another trade (reduce position)
    auto reduce_trade = create_trade(TreasuryType::Note_10Y, -3000000, 102.6);
    success = inventory_manager_->update_position(reduce_trade);
    EXPECT_TRUE(success);
    
    EXPECT_EQ(inventory_manager_->get_position(TreasuryType::Note_10Y), 7000000);
}

// Test VWAP calculation
TEST_F(InventoryManagerTest, VWAPCalculation) {
    // First trade
    auto trade1 = create_trade(TreasuryType::Note_10Y, 10000000, 102.5);
    inventory_manager_->update_position(trade1);
    
    auto position_info = inventory_manager_->get_position_info(TreasuryType::Note_10Y);
    EXPECT_DOUBLE_EQ(position_info.average_price, 102.5);
    
    // Second trade at different price
    auto trade2 = create_trade(TreasuryType::Note_10Y, 5000000, 102.0);
    inventory_manager_->update_position(trade2);
    
    position_info = inventory_manager_->get_position_info(TreasuryType::Note_10Y);
    // VWAP should be (10M * 102.5 + 5M * 102.0) / 15M = 102.33333...
    double expected_vwap = (10000000 * 102.5 + 5000000 * 102.0) / 15000000;
    EXPECT_NEAR(position_info.average_price, expected_vwap, 1e-6);
}

// Test DV01 calculations
TEST_F(InventoryManagerTest, DV01Calculations) {
    // Test different instruments with their DV01 characteristics
    std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M, TreasuryType::Bill_6M, TreasuryType::Note_2Y,
        TreasuryType::Note_5Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y
    };
    
    std::array<double, 6> expected_dv01_per_million = {98.0, 196.0, 196.0, 472.0, 867.0, 1834.0};
    
    for (size_t i = 0; i < 6; ++i) {
        auto trade = create_trade(instruments[i], 5000000, 102.0); // $5M position
        inventory_manager_->update_position(trade);
        
        auto position_info = inventory_manager_->get_position_info(instruments[i]);
        double expected_dv01 = 5.0 * expected_dv01_per_million[i]; // 5M / 1M * DV01_per_million
        
        EXPECT_NEAR(position_info.position_dv01, expected_dv01, 1.0)
            << "Failed for instrument " << i;
    }
}

// Test inventory adjustment calculations
TEST_F(InventoryManagerTest, InventoryAdjustmentCalculations) {
    // Start with no position
    auto adjustment = inventory_manager_->calculate_inventory_adjustment(TreasuryType::Note_10Y);
    EXPECT_DOUBLE_EQ(adjustment.bid_adjustment_bps, 0.0);
    EXPECT_DOUBLE_EQ(adjustment.ask_adjustment_bps, 0.0);
    EXPECT_DOUBLE_EQ(adjustment.size_multiplier, 1.0);
    EXPECT_FALSE(adjustment.should_stop_quoting);
    
    // Add a long position
    auto long_trade = create_trade(TreasuryType::Note_10Y, 20000000, 102.5); // $20M long
    inventory_manager_->update_position(long_trade);
    
    adjustment = inventory_manager_->calculate_inventory_adjustment(TreasuryType::Note_10Y);
    
    // Should penalize buying (negative bid adjustment) and favor selling (smaller ask adjustment)
    EXPECT_LT(adjustment.bid_adjustment_bps, 0.0);
    EXPECT_GT(adjustment.ask_adjustment_bps, 0.0);
    EXPECT_LT(adjustment.ask_adjustment_bps, std::abs(adjustment.bid_adjustment_bps));
    EXPECT_LT(adjustment.size_multiplier, 1.0); // Size should be reduced
    
    // Add a short position to another instrument
    auto short_trade = create_trade(TreasuryType::Note_5Y, -15000000, 101.5); // $15M short
    inventory_manager_->update_position(short_trade);
    
    auto short_adjustment = inventory_manager_->calculate_inventory_adjustment(TreasuryType::Note_5Y);
    
    // Should favor buying (smaller bid adjustment) and penalize selling (positive ask adjustment)
    EXPECT_GT(short_adjustment.bid_adjustment_bps, 0.0);
    EXPECT_GT(short_adjustment.ask_adjustment_bps, 0.0);
    EXPECT_LT(short_adjustment.bid_adjustment_bps, short_adjustment.ask_adjustment_bps);
}

// Test risk limits enforcement
TEST_F(InventoryManagerTest, RiskLimitsEnforcement) {
    // Test position limits
    bool can_trade = inventory_manager_->can_trade(TreasuryType::Note_10Y, 50000000); // $50M
    EXPECT_TRUE(can_trade);
    
    can_trade = inventory_manager_->can_trade(TreasuryType::Note_10Y, 150000000); // $150M (exceeds limit)
    EXPECT_FALSE(can_trade);
    
    // Add position close to limit
    auto large_trade = create_trade(TreasuryType::Note_10Y, 95000000, 102.5); // $95M
    inventory_manager_->update_position(large_trade);
    
    // Should not be able to add significant additional position
    can_trade = inventory_manager_->can_trade(TreasuryType::Note_10Y, 10000000); // Would exceed limit
    EXPECT_FALSE(can_trade);
    
    // But should be able to reduce position
    can_trade = inventory_manager_->can_trade(TreasuryType::Note_10Y, -10000000);
    EXPECT_TRUE(can_trade);
}

// Test DV01 limit enforcement
TEST_F(InventoryManagerTest, DV01LimitEnforcement) {
    // 30Y Bond has highest DV01 - test with that
    // $20M in 30Y bonds = 20 * 1834 = 36,680 DV01 (within limit of 50k)
    auto trade1 = create_trade(TreasuryType::Bond_30Y, 20000000, 103.0);
    inventory_manager_->update_position(trade1);
    
    bool can_trade = inventory_manager_->can_trade(TreasuryType::Bond_30Y, 5000000); // Additional $5M
    EXPECT_TRUE(can_trade); // Should still be within DV01 limit
    
    // Try to add position that would exceed DV01 limit
    can_trade = inventory_manager_->can_trade(TreasuryType::Bond_30Y, 15000000); // Would be 35M total = 64,190 DV01
    EXPECT_FALSE(can_trade); // Should exceed DV01 limit
}

// Test mark-to-market functionality
TEST_F(InventoryManagerTest, MarkToMarket) {
    // Set up positions in multiple instruments
    auto trade1 = create_trade(TreasuryType::Note_10Y, 10000000, 102.5);
    auto trade2 = create_trade(TreasuryType::Note_5Y, -5000000, 101.0);
    
    inventory_manager_->update_position(trade1);
    inventory_manager_->update_position(trade2);
    
    // Create current market prices
    std::array<Price32nd, 6> current_prices;
    current_prices[static_cast<size_t>(TreasuryType::Note_10Y)] = Price32nd::from_decimal(102.8); // Up 0.3
    current_prices[static_cast<size_t>(TreasuryType::Note_5Y)] = Price32nd::from_decimal(100.8);   // Down 0.2
    current_prices[static_cast<size_t>(TreasuryType::Note_2Y)] = Price32nd::from_decimal(101.0);
    current_prices[static_cast<size_t>(TreasuryType::Bill_3M)] = Price32nd::from_decimal(99.95);
    current_prices[static_cast<size_t>(TreasuryType::Bill_6M)] = Price32nd::from_decimal(99.90);
    current_prices[static_cast<size_t>(TreasuryType::Bond_30Y)] = Price32nd::from_decimal(104.0);
    
    // Perform mark-to-market
    inventory_manager_->mark_to_market(current_prices);
    
    // Check unrealized P&L
    auto position_info_10y = inventory_manager_->get_position_info(TreasuryType::Note_10Y);
    auto position_info_5y = inventory_manager_->get_position_info(TreasuryType::Note_5Y);
    
    // 10Y: Long 10M at 102.5, marked at 102.8 = profit of 0.3 * 10M = 3M cents
    EXPECT_NEAR(position_info_10y.unrealized_pnl, 3000000, 1000); // Within $1000
    
    // 5Y: Short 5M at 101.0, marked at 100.8 = profit of 0.2 * 5M = 1M cents (price went down, short profits)
    EXPECT_NEAR(position_info_5y.unrealized_pnl, 1000000, 1000); // Within $1000
    
    const auto& portfolio_state = inventory_manager_->get_portfolio_state();
    EXPECT_GT(portfolio_state.unrealized_pnl.load(), 0); // Total should be positive
}

// Test portfolio-level metrics
TEST_F(InventoryManagerTest, PortfolioMetrics) {
    // Add positions across multiple instruments
    auto trade1 = create_trade(TreasuryType::Note_10Y, 10000000, 102.5);   // $10M
    auto trade2 = create_trade(TreasuryType::Note_5Y, 15000000, 101.0);    // $15M
    auto trade3 = create_trade(TreasuryType::Bond_30Y, -8000000, 103.0);   // $8M short
    
    inventory_manager_->update_position(trade1);
    inventory_manager_->update_position(trade2);
    inventory_manager_->update_position(trade3);
    
    const auto& portfolio_state = inventory_manager_->get_portfolio_state();
    
    // Total notional should be sum of absolute positions
    EXPECT_EQ(portfolio_state.total_notional.load(), 33000000); // 10M + 15M + 8M
    
    // Portfolio DV01 should be sum of instrument DV01s
    double expected_portfolio_dv01 = 
        10.0 * 867.0 +    // 10Y Note
        15.0 * 472.0 +    // 5Y Note  
        8.0 * 1834.0;     // 30Y Bond
    
    EXPECT_NEAR(portfolio_state.portfolio_dv01.load(), expected_portfolio_dv01, 100.0);
    
    // Concentration ratio should be largest position / total
    double expected_concentration = 15000000.0 / 33000000.0; // 5Y is largest
    EXPECT_NEAR(portfolio_state.concentration_ratio.load(), expected_concentration, 0.01);
}

// Test emergency position control
TEST_F(InventoryManagerTest, EmergencyPositionControl) {
    // Build up to near position limit
    auto large_trade = create_trade(TreasuryType::Note_10Y, 96000000, 102.5); // $96M (96% of limit)
    inventory_manager_->update_position(large_trade);
    
    auto adjustment = inventory_manager_->calculate_inventory_adjustment(TreasuryType::Note_10Y);
    EXPECT_TRUE(adjustment.should_stop_quoting); // Should trigger emergency stop
    
    // Test DV01 emergency limit
    auto dv01_trade = create_trade(TreasuryType::Bond_30Y, 27000000, 103.0); // ~49.5k DV01 (99% of limit)
    inventory_manager_->update_position(dv01_trade);
    
    auto dv01_adjustment = inventory_manager_->calculate_inventory_adjustment(TreasuryType::Bond_30Y);
    EXPECT_TRUE(dv01_adjustment.should_stop_quoting); // Should trigger emergency stop
}

// Test performance requirements
TEST_F(InventoryManagerTest, PerformanceRequirements) {
    constexpr int num_iterations = 10000;
    std::vector<uint64_t> update_latencies;
    std::vector<uint64_t> adjustment_latencies;
    
    update_latencies.reserve(num_iterations);
    adjustment_latencies.reserve(num_iterations);
    
    // Test position update performance
    for (int i = 0; i < num_iterations; ++i) {
        auto trade = create_trade(TreasuryType::Note_10Y, 1000000, 102.5 + i * 0.001);
        
        auto start = std::chrono::high_resolution_clock::now();
        inventory_manager_->update_position(trade);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        update_latencies.push_back(latency);
    }
    
    // Test inventory adjustment performance
    for (int i = 0; i < num_iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto adjustment = inventory_manager_->calculate_inventory_adjustment(TreasuryType::Note_10Y);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        adjustment_latencies.push_back(latency);
    }
    
    // Calculate statistics
    std::sort(update_latencies.begin(), update_latencies.end());
    std::sort(adjustment_latencies.begin(), adjustment_latencies.end());
    
    auto update_median = update_latencies[num_iterations / 2];
    auto update_p95 = update_latencies[static_cast<size_t>(num_iterations * 0.95)];
    auto adjustment_median = adjustment_latencies[num_iterations / 2];
    auto adjustment_p95 = adjustment_latencies[static_cast<size_t>(num_iterations * 0.95)];
    
    // Performance assertions
    EXPECT_LE(update_median, 200);      // 200ns target for position updates
    EXPECT_LE(update_p95, 400);         // 95th percentile under 400ns
    EXPECT_LE(adjustment_median, 100);  // 100ns target for inventory adjustments
    EXPECT_LE(adjustment_p95, 200);     // 95th percentile under 200ns
    
    std::cout << "Position Update Latency Statistics:\n";
    std::cout << "  Median: " << update_median << "ns\n";
    std::cout << "  95th percentile: " << update_p95 << "ns\n";
    
    std::cout << "Inventory Adjustment Latency Statistics:\n";
    std::cout << "  Median: " << adjustment_median << "ns\n";
    std::cout << "  95th percentile: " << adjustment_p95 << "ns\n";
}

// Test state consistency and versioning
TEST_F(InventoryManagerTest, StateConsistency) {
    auto initial_state = inventory_manager_->get_portfolio_state();
    auto initial_version = initial_state.state_version.load();
    
    // Make several position changes
    for (int i = 0; i < 10; ++i) {
        auto trade = create_trade(
            static_cast<TreasuryType>(i % 6), 
            1000000 * (i + 1), 
            102.0 + i * 0.1
        );
        inventory_manager_->update_position(trade);
    }
    
    auto final_state = inventory_manager_->get_portfolio_state();
    auto final_version = final_state.state_version.load();
    
    // Version should have incremented
    EXPECT_GT(final_version, initial_version);
    
    // State should be internally consistent
    int64_t calculated_total_notional = 0;
    double calculated_total_dv01 = 0.0;
    
    for (size_t i = 0; i < 6; ++i) {
        auto position_info = inventory_manager_->get_position_info(static_cast<TreasuryType>(i));
        calculated_total_notional += position_info.gross_position;
        calculated_total_dv01 += position_info.position_dv01;
    }
    
    EXPECT_EQ(final_state.total_notional.load(), calculated_total_notional);
    EXPECT_NEAR(final_state.portfolio_dv01.load(), calculated_total_dv01, 1.0);
}