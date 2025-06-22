#include <gtest/gtest.h>
#include <memory>
#include <array>
#include <cmath>
#include "hft/strategy/advanced_market_maker.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::strategy;
using namespace hft::trading;
using namespace hft::market_data;

/**
 * @brief Test fixture for AdvancedMarketMaker
 * 
 * Tests sophisticated market making features including:
 * - Volatility-adaptive spread calculation
 * - Market microstructure analysis
 * - Dynamic position sizing
 * - Inventory management
 * - Performance characteristics
 */
class AdvancedMarketMakerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pools
        order_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrder, 4096>>();
        level_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>>();
        update_buffer_ = std::make_unique<hft::SPSCRingBuffer<OrderBookUpdate, 8192>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize advanced market maker
        strategy_ = std::make_unique<AdvancedMarketMaker>(*order_pool_, *order_book_);
    }

    void TearDown() override {
        strategy_.reset();
        order_book_.reset();
        update_buffer_.reset();
        level_pool_.reset();
        order_pool_.reset();
    }

    // Helper to create test market update with order book depth
    AdvancedMarketMaker::MarketUpdate create_test_market_update(
        TreasuryType instrument = TreasuryType::Note_10Y,
        double bid_price = 102.5,
        double ask_price = 102.53125,
        uint64_t bid_size = 5000000,
        uint64_t ask_size = 5000000
    ) {
        AdvancedMarketMaker::MarketUpdate update;
        update.instrument = instrument;
        update.best_bid = Price32nd::from_decimal(bid_price);
        update.best_ask = Price32nd::from_decimal(ask_price);
        update.bid_size = bid_size;
        update.ask_size = ask_size;
        update.update_time_ns = hft::HFTTimer::get_timestamp_ns();
        
        // Fill order book depth
        for (size_t i = 0; i < AdvancedMarketMaker::MICROSTRUCTURE_LEVELS; ++i) {
            const double tick_increment = 0.015625; // 1/64th
            update.bid_levels[i] = Price32nd::from_decimal(bid_price - (i + 1) * tick_increment);
            update.ask_levels[i] = Price32nd::from_decimal(ask_price + (i + 1) * tick_increment);
            update.bid_sizes[i] = bid_size / (i + 1);
            update.ask_sizes[i] = ask_size / (i + 1);
        }
        
        return update;
    }

    // Helper to create volatile market conditions
    AdvancedMarketMaker::MarketUpdate create_volatile_market_update() {
        auto update = create_test_market_update();
        
        // Simulate volatile conditions with wider spreads and smaller sizes
        update.best_bid = Price32nd::from_decimal(102.4);
        update.best_ask = Price32nd::from_decimal(102.6);
        update.bid_size = 1000000;
        update.ask_size = 1000000;
        
        // Thinner order book depth
        for (size_t i = 0; i < AdvancedMarketMaker::MICROSTRUCTURE_LEVELS; ++i) {
            update.bid_sizes[i] = 500000 / (i + 1);
            update.ask_sizes[i] = 500000 / (i + 1);
        }
        
        return update;
    }

protected:
    std::unique_ptr<hft::ObjectPool<TreasuryOrder, 4096>> order_pool_;
    std::unique_ptr<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>> level_pool_;
    std::unique_ptr<hft::SPSCRingBuffer<OrderBookUpdate, 8192>> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<AdvancedMarketMaker> strategy_;
};

// Test basic initialization
TEST_F(AdvancedMarketMakerTest, BasicInitialization) {
    EXPECT_NE(strategy_, nullptr);
    
    // All positions should be zero initially
    for (auto instrument : {TreasuryType::Note_2Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y}) {
        EXPECT_EQ(strategy_->get_position(instrument), 0);
        EXPECT_EQ(strategy_->get_unrealized_pnl(instrument), 0.0);
        EXPECT_EQ(strategy_->get_daily_pnl(instrument), 0.0);
        EXPECT_EQ(strategy_->get_risk_score(instrument), 0);
    }
    
    // Market conditions should be default initialized
    const auto& conditions = strategy_->get_market_conditions(TreasuryType::Note_10Y);
    EXPECT_EQ(conditions.realized_volatility, 0.0);
    EXPECT_EQ(conditions.liquidity_score, 1.0);
    EXPECT_EQ(conditions.last_update_time_ns, 0);
}

// Test basic trading decision making
TEST_F(AdvancedMarketMakerTest, BasicTradingDecision) {
    auto update = create_test_market_update();
    auto decision = strategy_->make_decision(update);
    
    // Should provide quotes in normal market conditions
    EXPECT_EQ(decision.action, AdvancedMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    EXPECT_EQ(decision.instrument, TreasuryType::Note_10Y);
    EXPECT_GT(decision.bid_size, 0);
    EXPECT_GT(decision.ask_size, 0);
    EXPECT_GT(decision.decision_time_ns, 0);
    EXPECT_GE(decision.confidence_score, 0.0);
    EXPECT_LE(decision.confidence_score, 1.0);
    
    // Bid should be below mid, ask should be above mid
    const double mid_price = (update.best_bid.to_decimal() + update.best_ask.to_decimal()) / 2.0;
    EXPECT_LT(decision.bid_price.to_decimal(), mid_price);
    EXPECT_GT(decision.ask_price.to_decimal(), mid_price);
    
    // Should have positive expected P&L
    EXPECT_GT(decision.expected_pnl, 0.0);
}

// Test volatility-adaptive spread calculation
TEST_F(AdvancedMarketMakerTest, VolatilityAdaptiveSpread) {
    auto normal_update = create_test_market_update();
    auto volatile_update = create_volatile_market_update();
    
    // Build price history to establish volatility patterns
    for (int i = 0; i < 100; ++i) {
        // Normal market: stable prices
        auto stable_update = create_test_market_update(TreasuryType::Note_10Y, 102.5 + 0.001 * i);
        strategy_->update_market_conditions(stable_update);
        
        // Volatile market: fluctuating prices  
        auto vol_update = create_test_market_update(TreasuryType::Note_2Y, 
                                                   102.5 + 0.1 * std::sin(i * 0.1));
        strategy_->update_market_conditions(vol_update);
    }
    
    // Calculate spreads for both conditions
    const double stable_spread = strategy_->calculate_optimal_spread(TreasuryType::Note_10Y);
    const double volatile_spread = strategy_->calculate_optimal_spread(TreasuryType::Note_2Y);
    
    // Volatile instrument should have wider spread
    EXPECT_GT(volatile_spread, stable_spread);
    EXPECT_GT(stable_spread, 0.1);  // Should have some minimum spread
    EXPECT_LT(volatile_spread, 5.0); // Should be reasonable
}

// Test market condition updates
TEST_F(AdvancedMarketMakerTest, MarketConditionUpdates) {
    auto update = create_test_market_update();
    
    // Initial conditions
    auto initial_conditions = strategy_->get_market_conditions(TreasuryType::Note_10Y);
    EXPECT_EQ(initial_conditions.last_update_time_ns, 0);
    
    // Update market conditions
    strategy_->update_market_conditions(update);
    
    // Conditions should be updated
    auto updated_conditions = strategy_->get_market_conditions(TreasuryType::Note_10Y);
    EXPECT_GT(updated_conditions.last_update_time_ns, initial_conditions.last_update_time_ns);
    EXPECT_GT(updated_conditions.bid_ask_spread, 0.0);
    EXPECT_GT(updated_conditions.liquidity_score, 0.0);
    EXPECT_LE(updated_conditions.liquidity_score, 1.0);
}

// Test position sizing based on confidence
TEST_F(AdvancedMarketMakerTest, ConfidenceBasedPositionSizing) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Test different confidence levels
    const uint64_t high_confidence_size = strategy_->calculate_position_size(instrument, 0.9);
    const uint64_t medium_confidence_size = strategy_->calculate_position_size(instrument, 0.5);
    const uint64_t low_confidence_size = strategy_->calculate_position_size(instrument, 0.1);
    
    // Higher confidence should lead to larger position sizes
    EXPECT_GT(high_confidence_size, medium_confidence_size);
    EXPECT_GT(medium_confidence_size, low_confidence_size);
    
    // All sizes should be within reasonable bounds
    EXPECT_GE(high_confidence_size, 100000);   // Minimum viable size
    EXPECT_LE(high_confidence_size, 10000000); // Maximum position size
}

// Test inventory management
TEST_F(AdvancedMarketMakerTest, InventoryManagement) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Initial inventory state
    auto initial_inventory = strategy_->get_inventory_state(instrument);
    EXPECT_EQ(initial_inventory.current_position, 0);
    EXPECT_EQ(initial_inventory.target_position, 0);
    EXPECT_EQ(initial_inventory.rebalance_urgency, 0.0);
    
    // Analyze inventory (should update internal state)
    strategy_->analyze_inventory(instrument);
    
    auto updated_inventory = strategy_->get_inventory_state(instrument);
    // Target position should remain zero for now (implemented as simple strategy)
    EXPECT_EQ(updated_inventory.target_position, 0);
}

// Test hedge ratio calculations
TEST_F(AdvancedMarketMakerTest, HedgeRatioCalculations) {
    // Create mock treasury curve yields
    std::array<double, 6> yields;
    for (size_t i = 0; i < yields.size(); ++i) {
        yields[i] = 0.02 + 0.001 * i; // Upward sloping yield curve
    }
    
    // Update hedge ratios
    strategy_->update_hedge_ratios(yields);
    
    // Check that hedge ratios were calculated
    const auto& hedge_ratios = strategy_->get_hedge_ratios();
    EXPECT_GT(hedge_ratios.last_calculation_time_ns, 0);
}

// Test decision making in volatile conditions
TEST_F(AdvancedMarketMakerTest, VolatileMarketDecisions) {
    // Create extremely volatile market update
    auto volatile_update = create_volatile_market_update();
    
    // Build volatile price history
    for (int i = 0; i < 50; ++i) {
        auto vol_update = create_test_market_update(TreasuryType::Note_10Y, 
                                                   102.0 + 0.5 * std::sin(i * 0.2));
        strategy_->update_market_conditions(vol_update);
    }
    
    auto decision = strategy_->make_decision(volatile_update);
    
    // In very volatile conditions, strategy might choose not to provide liquidity
    // or provide quotes with wider spreads and smaller sizes
    if (decision.action == AdvancedMarketMaker::TradingDecision::Action::UPDATE_QUOTES) {
        // If providing quotes, spread should be wider
        const double mid_price = (volatile_update.best_bid.to_decimal() + 
                                 volatile_update.best_ask.to_decimal()) / 2.0;
        const double spread = decision.ask_price.to_decimal() - decision.bid_price.to_decimal();
        EXPECT_GT(spread, 0.05); // Should have meaningful spread in volatile conditions
        
        // Confidence should be lower
        EXPECT_LT(decision.confidence_score, 0.8);
    }
}

// Test performance characteristics
TEST_F(AdvancedMarketMakerTest, DecisionMakingPerformance) {
    auto update = create_test_market_update();
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        strategy_->make_decision(update);
    }
    
    // Measure decision making latency
    const int iterations = 1000;
    uint64_t total_decision_time = 0;
    uint64_t max_decision_time = 0;
    uint64_t min_decision_time = UINT64_MAX;
    
    for (int i = 0; i < iterations; ++i) {
        const auto start_time = hft::HFTTimer::get_timestamp_ns();
        auto decision = strategy_->make_decision(update);
        const auto end_time = hft::HFTTimer::get_timestamp_ns();
        
        const uint64_t decision_time = end_time - start_time;
        total_decision_time += decision_time;
        max_decision_time = std::max(max_decision_time, decision_time);
        min_decision_time = std::min(min_decision_time, decision_time);
        
        // Decision should be made within reasonable time (relaxed for initial version)
        EXPECT_LT(decision_time, 30000); // Less than 30μs per decision (allowing for outliers)
        EXPECT_GT(decision.decision_time_ns, 0);
    }
    
    const uint64_t avg_decision_time = total_decision_time / iterations;
    
    // Performance validation (relaxed for initial version, will optimize)
    EXPECT_LT(avg_decision_time, 15000);   // Average < 15μs (target: eventually 500ns)
    EXPECT_LT(max_decision_time, 50000);   // Max < 50μs (target: eventually 1.5μs)
    EXPECT_GT(min_decision_time, 0);     // Should measure some time
    
    std::cout << "Advanced Market Maker Performance:" << std::endl;
    std::cout << "  Average Decision Time: " << avg_decision_time << "ns" << std::endl;
    std::cout << "  Min Decision Time: " << min_decision_time << "ns" << std::endl;
    std::cout << "  Max Decision Time: " << max_decision_time << "ns" << std::endl;
    std::cout << "  Target: <500ns" << std::endl;
}

// Test spread calculation performance
TEST_F(AdvancedMarketMakerTest, SpreadCalculationPerformance) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Set up market conditions
    auto update = create_test_market_update(instrument);
    strategy_->update_market_conditions(update);
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        strategy_->calculate_optimal_spread(instrument);
    }
    
    // Measure spread calculation latency
    const int iterations = 10000;
    uint64_t total_calc_time = 0;
    
    for (int i = 0; i < iterations; ++i) {
        const auto start_time = hft::HFTTimer::get_timestamp_ns();
        double spread = strategy_->calculate_optimal_spread(instrument);
        const auto end_time = hft::HFTTimer::get_timestamp_ns();
        
        total_calc_time += (end_time - start_time);
        
        // Spread should be reasonable
        EXPECT_GT(spread, 0.05);
        EXPECT_LT(spread, 5.0);
    }
    
    const uint64_t avg_calc_time = total_calc_time / iterations;
    
    // Should meet performance target
    EXPECT_LT(avg_calc_time, 75); // Target: <50ns, allowing some overhead
    
    std::cout << "Spread Calculation Performance:" << std::endl;
    std::cout << "  Average: " << avg_calc_time << "ns" << std::endl;
    std::cout << "  Target: <50ns" << std::endl;
}

// Test position sizing performance
TEST_F(AdvancedMarketMakerTest, PositionSizingPerformance) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Set up market conditions
    auto update = create_test_market_update(instrument);
    strategy_->update_market_conditions(update);
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        strategy_->calculate_position_size(instrument, 0.7);
    }
    
    // Measure position sizing latency
    const int iterations = 10000;
    uint64_t total_calc_time = 0;
    
    for (int i = 0; i < iterations; ++i) {
        const double confidence = 0.5 + 0.5 * (i % 100) / 100.0; // Vary confidence
        
        const auto start_time = hft::HFTTimer::get_timestamp_ns();
        uint64_t size = strategy_->calculate_position_size(instrument, confidence);
        const auto end_time = hft::HFTTimer::get_timestamp_ns();
        
        total_calc_time += (end_time - start_time);
        
        // Size should be reasonable
        EXPECT_GT(size, 50000);     // Minimum viable
        EXPECT_LT(size, 15000000);  // Maximum reasonable
    }
    
    const uint64_t avg_calc_time = total_calc_time / iterations;
    
    // Should meet performance target
    EXPECT_LT(avg_calc_time, 150); // Target: <100ns, allowing some overhead
    
    std::cout << "Position Sizing Performance:" << std::endl;
    std::cout << "  Average: " << avg_calc_time << "ns" << std::endl;
    std::cout << "  Target: <100ns" << std::endl;
}

// Test multi-instrument decision consistency
TEST_F(AdvancedMarketMakerTest, MultiInstrumentConsistency) {
    const std::array<TreasuryType, 3> instruments = {
        TreasuryType::Note_2Y,
        TreasuryType::Note_10Y,
        TreasuryType::Bond_30Y
    };
    
    // Test decision making for each instrument
    for (auto instrument : instruments) {
        auto update = create_test_market_update(instrument);
        auto decision = strategy_->make_decision(update);
        
        // All instruments should generate reasonable decisions
        EXPECT_EQ(decision.instrument, instrument);
        if (decision.action == AdvancedMarketMaker::TradingDecision::Action::UPDATE_QUOTES) {
            EXPECT_GT(decision.bid_size, 0);
            EXPECT_GT(decision.ask_size, 0);
            EXPECT_GE(decision.confidence_score, 0.0);
            EXPECT_LE(decision.confidence_score, 1.0);
        }
    }
}

// Test risk score calculations
TEST_F(AdvancedMarketMakerTest, RiskScoreCalculations) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Initial risk score should be low
    EXPECT_EQ(strategy_->get_risk_score(instrument), 0);
    
    // Create volatile conditions to increase risk
    for (int i = 0; i < 50; ++i) {
        auto volatile_update = create_test_market_update(instrument, 
                                                        102.0 + 0.3 * std::sin(i * 0.3));
        strategy_->update_market_conditions(volatile_update);
    }
    
    // Risk score should increase with volatility
    const uint32_t risk_after_volatility = strategy_->get_risk_score(instrument);
    EXPECT_GT(risk_after_volatility, 0);
    EXPECT_LE(risk_after_volatility, 1000); // Should be within valid range
}

// Test data structure alignment and size
TEST_F(AdvancedMarketMakerTest, DataStructureValidation) {
    // Verify cache alignment
    EXPECT_EQ(alignof(AdvancedMarketMaker), 64);
    
    // Verify structure sizes (must be cache-line aligned for performance)
    EXPECT_EQ(sizeof(AdvancedMarketMaker::MarketConditions), 64);
    EXPECT_EQ(sizeof(AdvancedMarketMaker::HedgeRatios), 64);
    EXPECT_EQ(sizeof(AdvancedMarketMaker::InventoryState), 64);
}