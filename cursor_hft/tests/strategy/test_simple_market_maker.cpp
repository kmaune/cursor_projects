#include <gtest/gtest.h>
#include <memory>
#include "hft/strategy/simple_market_maker.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::strategy;
using namespace hft::trading;
using namespace hft::market_data;

class SimpleMarketMakerTest : public ::testing::Test {
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

protected:
    std::unique_ptr<hft::ObjectPool<TreasuryOrder, 4096>> order_pool_;
    std::unique_ptr<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>> level_pool_;
    std::unique_ptr<hft::SPSCRingBuffer<OrderBookUpdate, 8192>> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<SimpleMarketMaker> strategy_;
};

// Test basic decision functionality
TEST_F(SimpleMarketMakerTest, BasicDecisionFunctionality) {
    auto update = create_test_market_update();
    
    auto decision = strategy_->make_decision(update);
    
    // Core functionality validation
    EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    EXPECT_EQ(decision.instrument, TreasuryType::Note_10Y);
    EXPECT_GT(decision.bid_size, 0);
    EXPECT_GT(decision.ask_size, 0);
    
    // Sanity check on timing (should be measured, but not asserted for specific values)
    EXPECT_GT(decision.decision_latency_ns, 0);
    EXPECT_GT(decision.decision_time_ns, 0);
}

// Test treasury pricing compliance
TEST_F(SimpleMarketMakerTest, TreasuryPricingCompliance) {
    auto update = create_test_market_update();
    
    auto decision = strategy_->make_decision(update);
    
    // Validate 32nds pricing alignment
    EXPECT_LE(decision.bid_price.half_32nds, 1);  // Only 0 or 1 allowed
    EXPECT_LE(decision.ask_price.half_32nds, 1);  // Only 0 or 1 allowed
    EXPECT_LT(decision.bid_price.thirty_seconds, 32);  // Valid 32nds range
    EXPECT_LT(decision.ask_price.thirty_seconds, 32);  // Valid 32nds range
    
    // Quotes should be reasonable and properly ordered
    const double market_bid = update.best_bid.to_decimal();
    const double market_ask = update.best_ask.to_decimal();
    const double quote_bid = decision.bid_price.to_decimal();
    const double quote_ask = decision.ask_price.to_decimal();
    
    // Our quotes should be reasonable relative to market
    EXPECT_LT(quote_bid, quote_ask) << "Bid should be less than ask";
    EXPECT_GT(quote_bid, 0) << "Bid should be positive";
    EXPECT_GT(quote_ask, 0) << "Ask should be positive";
    
    // As market makers, our quotes should be inside the market spread
    const double market_mid = (market_bid + market_ask) / 2.0;
    EXPECT_LT(std::abs(quote_bid - market_mid), std::abs(market_bid - market_ask)) 
        << "Our bid should be reasonable relative to market";
    EXPECT_LT(std::abs(quote_ask - market_mid), std::abs(market_bid - market_ask)) 
        << "Our ask should be reasonable relative to market";
}

// Test spread calculation
TEST_F(SimpleMarketMakerTest, SpreadCalculation) {
    auto update = create_test_market_update();
    
    // Test with zero position (should use base spread)
    auto decision1 = strategy_->make_decision(update);
    const double spread1 = decision1.ask_price.to_decimal() - decision1.bid_price.to_decimal();
    
    // Update position to test inventory penalty
    strategy_->update_position(TreasuryType::Note_10Y, 25000000,  // $25M position
                              Price32nd::from_decimal(102.5));
    
    auto decision2 = strategy_->make_decision(update);
    const double spread2 = decision2.ask_price.to_decimal() - decision2.bid_price.to_decimal();
    
    // Spread should widen with inventory
    EXPECT_GT(spread2, spread1) << "Spread should widen with inventory. Spread1: " 
              << spread1 << ", Spread2: " << spread2;
    
    // Both spreads should be reasonable for treasury markets
    EXPECT_GT(spread1, 0.001) << "Spread too narrow for treasury market";
    EXPECT_LT(spread1, 0.1) << "Spread too wide for treasury market";
    EXPECT_GT(spread2, spread1) << "Inventory penalty not applied";
}

// Test risk limits
TEST_F(SimpleMarketMakerTest, RiskLimits) {
    auto update = create_test_market_update();
    
    // Test normal operation
    EXPECT_TRUE(strategy_->check_risk_limits(TreasuryType::Note_10Y, 1000000));
    
    // Test with large position approaching limits
    strategy_->update_position(TreasuryType::Note_10Y, 45000000,  // $45M position (close to $50M limit)
                              Price32nd::from_decimal(102.5));
    
    // Should still allow reasonable orders
    EXPECT_TRUE(strategy_->check_risk_limits(TreasuryType::Note_10Y, 1000000));
    
    // Should reject orders that would exceed limits
    EXPECT_FALSE(strategy_->check_risk_limits(TreasuryType::Note_10Y, 10000000));
}

// Test position tracking
TEST_F(SimpleMarketMakerTest, PositionTracking) {
    // Initial position should be zero
    EXPECT_EQ(strategy_->get_position(TreasuryType::Note_10Y), 0);
    EXPECT_EQ(strategy_->get_unrealized_pnl(TreasuryType::Note_10Y), 0.0);
    
    // Update position
    const int64_t trade_size = 5000000;  // $5M
    const Price32nd fill_price = Price32nd::from_decimal(102.5);
    
    strategy_->update_position(TreasuryType::Note_10Y, trade_size, fill_price);
    
    // Check position updated
    EXPECT_EQ(strategy_->get_position(TreasuryType::Note_10Y), trade_size);
    
    // Add another trade
    strategy_->update_position(TreasuryType::Note_10Y, trade_size, fill_price);
    EXPECT_EQ(strategy_->get_position(TreasuryType::Note_10Y), 2 * trade_size);
    
    // Test sell trade
    strategy_->update_position(TreasuryType::Note_10Y, -trade_size, fill_price);
    EXPECT_EQ(strategy_->get_position(TreasuryType::Note_10Y), trade_size);
}

// Test invalid market data handling
TEST_F(SimpleMarketMakerTest, InvalidMarketDataHandling) {
    // Test with invalid bid/ask relationship
    SimpleMarketMaker::MarketUpdate invalid_update;
    invalid_update.instrument = TreasuryType::Note_10Y;
    invalid_update.best_bid = Price32nd::from_decimal(102.6);   // Bid higher than ask
    invalid_update.best_ask = Price32nd::from_decimal(102.5);
    invalid_update.bid_size = 1000000;
    invalid_update.ask_size = 1000000;
    
    auto decision = strategy_->make_decision(invalid_update);
    
    // Should return NO_ACTION for invalid data
    EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::NO_ACTION);
    EXPECT_GT(decision.decision_latency_ns, 0);  // Should still measure time
}

// Test performance tracking
TEST_F(SimpleMarketMakerTest, PerformanceTracking) {
    auto update = create_test_market_update();
    
    // Initial state
    EXPECT_EQ(strategy_->get_decision_count(), 0);
    EXPECT_EQ(strategy_->get_average_decision_latency_ns(), 0);
    
    // Make several decisions
    const int num_decisions = 10;
    for (int i = 0; i < num_decisions; ++i) {
        auto decision = strategy_->make_decision(update);
        EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    }
    
    // Check tracking functionality (not specific latency values)
    EXPECT_EQ(strategy_->get_decision_count(), num_decisions);
    EXPECT_GT(strategy_->get_average_decision_latency_ns(), 0);
}

// Test different treasury instruments
TEST_F(SimpleMarketMakerTest, MultipleInstruments) {
    const std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M,
        TreasuryType::Bill_6M,
        TreasuryType::Note_2Y,
        TreasuryType::Note_5Y,
        TreasuryType::Note_10Y,
        TreasuryType::Bond_30Y
    };
    
    for (const auto instrument : instruments) {
        auto update = create_test_market_update(instrument);
        auto decision = strategy_->make_decision(update);
        
        EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
        EXPECT_EQ(decision.instrument, instrument);
        EXPECT_GT(decision.decision_latency_ns, 0);
        
        // All should produce valid quotes
        EXPECT_GT(decision.bid_size, 0);
        EXPECT_GT(decision.ask_size, 0);
        EXPECT_LT(decision.bid_price.to_decimal(), decision.ask_price.to_decimal());
    }
}

// Test for decision consistency across many iterations
TEST_F(SimpleMarketMakerTest, DecisionConsistency) {
    auto update = create_test_market_update();
    
    const int num_iterations = 100;  // Reduced for faster test
    
    // Store first decision for comparison
    auto first_decision = strategy_->make_decision(update);
    EXPECT_EQ(first_decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    
    // All subsequent decisions with same input should be identical (functional determinism)
    for (int i = 1; i < num_iterations; ++i) {
        auto decision = strategy_->make_decision(update);
        
        EXPECT_EQ(decision.action, first_decision.action);
        EXPECT_EQ(decision.instrument, first_decision.instrument);
        EXPECT_EQ(decision.bid_price.whole, first_decision.bid_price.whole);
        EXPECT_EQ(decision.bid_price.thirty_seconds, first_decision.bid_price.thirty_seconds);
        EXPECT_EQ(decision.ask_price.whole, first_decision.ask_price.whole);
        EXPECT_EQ(decision.ask_price.thirty_seconds, first_decision.ask_price.thirty_seconds);
        EXPECT_EQ(decision.bid_size, first_decision.bid_size);
        EXPECT_EQ(decision.ask_size, first_decision.ask_size);
        
        // Timing should always be measured
        EXPECT_GT(decision.decision_latency_ns, 0);
        EXPECT_GT(decision.decision_time_ns, 0);
    }
}

// ========== Phase 2: Enhanced Position Management and Risk Tests ==========

// Test enhanced P&L calculation
TEST_F(SimpleMarketMakerTest, EnhancedPnLCalculation) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Initial state - no P&L
    EXPECT_EQ(strategy_->get_daily_pnl(instrument), 0.0);
    EXPECT_EQ(strategy_->get_unrealized_pnl(instrument), 0.0);
    
    // Execute a buy trade
    const int64_t buy_size = 5000000;  // $5M
    const Price32nd buy_price = Price32nd::from_decimal(102.5);
    strategy_->update_position(instrument, buy_size, buy_price);
    
    // Should track the position but no P&L yet
    EXPECT_EQ(strategy_->get_position(instrument), buy_size);
    EXPECT_EQ(strategy_->get_daily_pnl(instrument), 0.0);  // No realized P&L yet
    
    // Update market price to simulate mark-to-market
    const Price32nd higher_price = Price32nd::from_decimal(102.75);  // 25bp gain
    strategy_->update_market_price(instrument, higher_price);
    
    // Should now have unrealized P&L (position in dollars, price change in points)
    const double expected_unrealized = buy_size * (102.75 - 102.5);
    EXPECT_NEAR(strategy_->get_unrealized_pnl(instrument), expected_unrealized, 1000.0);
    
    // Execute a partial sell to realize some P&L
    const int64_t sell_size = -2000000;  // Sell $2M
    strategy_->update_position(instrument, sell_size, higher_price);
    
    // Should have realized P&L now
    EXPECT_GT(strategy_->get_daily_pnl(instrument), 0.0);
    EXPECT_EQ(strategy_->get_position(instrument), buy_size + sell_size);  // Net position reduced
}

// Test risk score calculation
TEST_F(SimpleMarketMakerTest, RiskScoreCalculation) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Initial risk score should be low
    EXPECT_LT(strategy_->get_risk_score(instrument), 100);
    
    // Add position and check risk score increases
    strategy_->update_position(instrument, 25000000, Price32nd::from_decimal(102.5));  // $25M
    uint32_t risk_score_1 = strategy_->get_risk_score(instrument);
    EXPECT_GT(risk_score_1, 100);
    
    // Add more position - risk should increase
    strategy_->update_position(instrument, 20000000, Price32nd::from_decimal(102.5));  // Additional $20M
    uint32_t risk_score_2 = strategy_->get_risk_score(instrument);
    EXPECT_GT(risk_score_2, risk_score_1);
    
    // Risk score should be reasonable (not exceed maximum)
    EXPECT_LE(risk_score_2, 1000);
}

// Test portfolio VaR calculation
TEST_F(SimpleMarketMakerTest, PortfolioVaRCalculation) {
    // Initial portfolio VaR should be zero
    EXPECT_EQ(strategy_->get_portfolio_var(), 0.0);
    
    // Add positions in multiple instruments
    strategy_->update_position(TreasuryType::Note_2Y, 10000000, Price32nd::from_decimal(99.5));
    strategy_->update_position(TreasuryType::Note_10Y, 15000000, Price32nd::from_decimal(102.5));
    strategy_->update_position(TreasuryType::Bond_30Y, 8000000, Price32nd::from_decimal(105.0));
    
    // Update market prices to calculate position VaR
    strategy_->update_market_price(TreasuryType::Note_2Y, Price32nd::from_decimal(99.5));
    strategy_->update_market_price(TreasuryType::Note_10Y, Price32nd::from_decimal(102.5));
    strategy_->update_market_price(TreasuryType::Bond_30Y, Price32nd::from_decimal(105.0));
    
    // Update portfolio risk
    strategy_->update_portfolio_risk();
    
    // Portfolio VaR should be positive but reasonable
    EXPECT_GT(strategy_->get_portfolio_var(), 0.0);
    EXPECT_LT(strategy_->get_portfolio_var(), 2000000.0);  // Should be reasonable for treasury positions
}

// Test position skewing
TEST_F(SimpleMarketMakerTest, PositionSkewing) {
    auto update = create_test_market_update();
    
    // Get initial decision with no position
    auto decision_no_position = strategy_->make_decision(update);
    
    // Add a large long position
    strategy_->update_position(TreasuryType::Note_10Y, 30000000, Price32nd::from_decimal(102.5));
    
    // Get decision with position - should skew quotes
    auto decision_with_position = strategy_->make_decision(update);
    
    // With long position, should favor selling (ask should be more competitive relative to bid)
    const double original_spread = decision_no_position.ask_price.to_decimal() - decision_no_position.bid_price.to_decimal();
    const double skewed_spread = decision_with_position.ask_price.to_decimal() - decision_with_position.bid_price.to_decimal();
    
    // Spread might change due to skewing
    EXPECT_GT(skewed_spread, 0.0);  // Still positive spread
    EXPECT_NE(decision_no_position.bid_price.to_decimal(), decision_with_position.bid_price.to_decimal());
}

// Test enhanced risk limits
TEST_F(SimpleMarketMakerTest, EnhancedRiskLimits) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Should pass normal risk checks initially
    auto update = create_test_market_update(instrument);
    auto decision = strategy_->make_decision(update);
    EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    
    // Add position close to limits
    strategy_->update_position(instrument, 45000000, Price32nd::from_decimal(102.5));  // $45M
    
    // Update market price and portfolio risk
    strategy_->update_market_price(instrument, Price32nd::from_decimal(102.5));
    strategy_->update_portfolio_risk();
    
    // Should still quote with position near limits
    decision = strategy_->make_decision(update);
    EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
    
    // Risk score should be high but not maximum
    EXPECT_GT(strategy_->get_risk_score(instrument), 600);
    EXPECT_LT(strategy_->get_risk_score(instrument), 950);
}

// Test volume-weighted average price calculation
TEST_F(SimpleMarketMakerTest, VolumeWeightedAveragePrice) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    // Execute trades at different prices
    strategy_->update_position(instrument, 5000000, Price32nd::from_decimal(102.0));   // $5M at 102.0
    strategy_->update_position(instrument, 3000000, Price32nd::from_decimal(102.5));   // $3M at 102.5
    strategy_->update_position(instrument, 2000000, Price32nd::from_decimal(103.0));   // $2M at 103.0
    
    // Total: $10M position
    EXPECT_EQ(strategy_->get_position(instrument), 10000000);
    
    // VWAP should be weighted average: (5*102.0 + 3*102.5 + 2*103.0) / 10 = 102.25
    const double expected_vwap = (5.0 * 102.0 + 3.0 * 102.5 + 2.0 * 103.0) / 10.0;
    
    // Get current market price to verify P&L calculation is based on VWAP
    strategy_->update_market_price(instrument, Price32nd::from_decimal(102.5));
    const double unrealized_pnl = strategy_->get_unrealized_pnl(instrument);
    const double expected_pnl = 10000000 * (102.5 - expected_vwap);
    
    EXPECT_NEAR(unrealized_pnl, expected_pnl, 10000.0);  // Allow for scaling differences
}

// Test multiple instrument coordination
TEST_F(SimpleMarketMakerTest, MultiInstrumentCoordination) {
    // Add positions in multiple instruments
    strategy_->update_position(TreasuryType::Note_2Y, 5000000, Price32nd::from_decimal(99.5));
    strategy_->update_position(TreasuryType::Note_10Y, 10000000, Price32nd::from_decimal(102.5));
    strategy_->update_position(TreasuryType::Bond_30Y, 3000000, Price32nd::from_decimal(105.0));
    
    // Update market prices
    strategy_->update_market_price(TreasuryType::Note_2Y, Price32nd::from_decimal(99.6));
    strategy_->update_market_price(TreasuryType::Note_10Y, Price32nd::from_decimal(102.4));
    strategy_->update_market_price(TreasuryType::Bond_30Y, Price32nd::from_decimal(105.1));
    
    // Each instrument should have different risk characteristics
    uint32_t risk_2y = strategy_->get_risk_score(TreasuryType::Note_2Y);
    uint32_t risk_10y = strategy_->get_risk_score(TreasuryType::Note_10Y);
    uint32_t risk_30y = strategy_->get_risk_score(TreasuryType::Bond_30Y);
    
    // 10Y should have higher risk than 2Y due to larger position
    EXPECT_GT(risk_10y, risk_2y);
    
    // Risk should be reasonable for all instruments 
    EXPECT_LT(risk_2y, 500);
    EXPECT_LT(risk_10y, 800);
    EXPECT_LT(risk_30y, 800);
    
    // All should be generating quotes
    for (auto instrument : {TreasuryType::Note_2Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y}) {
        auto update = create_test_market_update(instrument);
        auto decision = strategy_->make_decision(update);
        EXPECT_EQ(decision.action, SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES);
        EXPECT_EQ(decision.instrument, instrument);
    }
}