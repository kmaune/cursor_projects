#include <gtest/gtest.h>
#include "hft/market_data/venue_simulation.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"
#include <memory>
#include <vector>

using namespace hft::market_data;

class VenueSimulationTest : public ::testing::Test {
protected:
    void SetUp() override {
        venue_ = std::make_unique<PrimaryDealerVenue>();
    }
    
    std::unique_ptr<PrimaryDealerVenue> venue_;
    
    // Helper to create a basic order
    TreasuryOrder create_test_order(
        uint64_t order_id,
        OrderSide side,
        double price,
        uint64_t quantity) {
        TreasuryOrder order{};
        order.order_id = order_id;
        order.client_order_id = order_id * 100;
        order.timestamp_created_ns = hft::HFTTimer::get_timestamp_ns();
        order.instrument_type = TreasuryType::Note_10Y;
        order.order_type = OrderType::Limit;
        order.side = side;
        order.limit_price = Price32nd::from_decimal(price);
        order.quantity = quantity;
        order.remaining_quantity = quantity;
        return order;
    }
    
    // Helper to create a market tick
    TreasuryTick create_test_tick(double bid, double ask, uint64_t size) {
        TreasuryTick tick{};
        tick.instrument_type = TreasuryType::Note_10Y;
        tick.timestamp_ns = hft::HFTTimer::get_timestamp_ns();
        tick.bid_price = Price32nd::from_decimal(bid);
        tick.ask_price = Price32nd::from_decimal(ask);
        tick.bid_size = size;
        tick.ask_size = size;
        tick.bid_yield = YieldCalculator::price_to_yield(
            TreasuryInstrument(tick.instrument_type, 0, 0), tick.bid_price, 0);
        tick.ask_yield = YieldCalculator::price_to_yield(
            TreasuryInstrument(tick.instrument_type, 0, 0), tick.ask_price, 0);
        return tick;
    }
};

// Test order submission and acknowledgment
TEST_F(VenueSimulationTest, OrderSubmissionAndAcknowledgment) {
    auto order = create_test_order(1, OrderSide::Buy, 99.0, 10);
    ASSERT_TRUE(venue_->submit_order(order));
    
    // Should get acknowledgment
    VenueResponse response{};
    ASSERT_EQ(venue_->get_venue_responses(&response, 1), 1);
    EXPECT_EQ(response.order_id, order.order_id);
    EXPECT_EQ(response.new_status, OrderStatus::Acknowledged);
    EXPECT_GT(response.timestamp_venue_ns, order.timestamp_created_ns);
    
    // Verify venue stats
    auto stats = venue_->get_venue_stats();
    EXPECT_EQ(stats.orders_submitted, 1);
    EXPECT_EQ(stats.orders_acknowledged, 1);
    EXPECT_EQ(stats.orders_rejected, 0);
}

// Test order rejection
TEST_F(VenueSimulationTest, OrderRejection) {
    // Invalid order (zero quantity)
    auto order = create_test_order(1, OrderSide::Buy, 99.0, 0);
    ASSERT_TRUE(venue_->submit_order(order));
    
    // Should get rejection
    VenueResponse response{};
    ASSERT_EQ(venue_->get_venue_responses(&response, 1), 1);
    EXPECT_EQ(response.order_id, order.order_id);
    EXPECT_EQ(response.new_status, OrderStatus::Rejected);
    
    // Verify venue stats
    auto stats = venue_->get_venue_stats();
    EXPECT_EQ(stats.orders_submitted, 1);
    EXPECT_EQ(stats.orders_acknowledged, 0);
    EXPECT_EQ(stats.orders_rejected, 1);
}

// Test order cancellation
TEST_F(VenueSimulationTest, OrderCancellation) {
    auto order = create_test_order(1, OrderSide::Buy, 99.0, 10);
    ASSERT_TRUE(venue_->submit_order(order));
    
    // Process acknowledgment
    VenueResponse response{};
    ASSERT_EQ(venue_->get_venue_responses(&response, 1), 1);
    EXPECT_EQ(response.new_status, OrderStatus::Acknowledged);
    
    // Cancel order
    ASSERT_TRUE(venue_->cancel_order(order.order_id));
    
    // Should get cancellation confirmation
    ASSERT_EQ(venue_->get_venue_responses(&response, 1), 1);
    EXPECT_EQ(response.order_id, order.order_id);
    EXPECT_EQ(response.new_status, OrderStatus::Cancelled);
    
    // Verify venue stats
    auto stats = venue_->get_venue_stats();
    EXPECT_EQ(stats.orders_cancelled, 1);
}

// Test fill generation
TEST_F(VenueSimulationTest, FillGeneration) {
    // Submit aggressive buy order
    auto order = create_test_order(1, OrderSide::Buy, 99.5, 10);
    ASSERT_TRUE(venue_->submit_order(order));
    
    // Process acknowledgment
    VenueResponse response{};
    ASSERT_EQ(venue_->get_venue_responses(&response, 1), 1);
    EXPECT_EQ(response.new_status, OrderStatus::Acknowledged);
    
    // Send market tick that should trigger fill
    auto tick = create_test_tick(99.0, 99.25, 100);
    venue_->process_market_update(tick);
    
    // Should get fill
    ASSERT_EQ(venue_->get_venue_responses(&response, 1), 1);
    EXPECT_EQ(response.order_id, order.order_id);
    EXPECT_TRUE(response.new_status == OrderStatus::Filled ||
                response.new_status == OrderStatus::PartiallyFilled);
    EXPECT_GT(response.fill_quantity, 0);
    EXPECT_LE(response.fill_price.to_decimal(), 99.25);  // Fill at or better than ask
    
    // Verify venue stats
    auto stats = venue_->get_venue_stats();
    EXPECT_GT(stats.orders_filled + stats.orders_partially_filled, 0);
}

// Test order router functionality
TEST_F(VenueSimulationTest, OrderRouterFunctionality) {
    TreasuryOrderRouter router;
    
    // Add multiple venues
    for (int i = 0; i < 3; ++i) {
        router.add_venue(std::make_unique<PrimaryDealerVenue>(
            "Venue" + std::to_string(i)));
    }
    
    // Submit orders
    std::vector<TreasuryOrder> orders;
    for (int i = 0; i < 10; ++i) {
        auto order = create_test_order(i + 1, OrderSide::Buy, 99.0, 10);
        ASSERT_TRUE(router.route_order(order));
        orders.push_back(order);
    }
    
    // Process responses
    size_t ack_count = 0;
    VenueResponse responses[256];
    while (router.process_venue_responses() > 0) {
        size_t count = router.get_fills(responses, 256);
        ack_count += count;
    }
    
    EXPECT_EQ(ack_count, orders.size());
}

// Test market making strategy
TEST_F(VenueSimulationTest, MarketMakingStrategy) {
    TreasuryOrderRouter router;
    router.add_venue(std::make_unique<PrimaryDealerVenue>());
    
    TestMarketMakingStrategy strategy(router);
    
    // Process market tick
    auto tick = create_test_tick(99.0, 99.25, 100);
    strategy.process_market_tick(tick);
    
    // Process responses
    router.process_venue_responses();
    
    VenueResponse fills[256];
    size_t fill_count = router.get_fills(fills, 256);
    for (size_t i = 0; i < fill_count; ++i) {
        strategy.process_fill(fills[i]);
    }
    
    // Verify strategy stats
    auto stats = strategy.get_strategy_stats();
    EXPECT_GT(stats.orders_sent, 0);
}

// Test yield-based orders
TEST_F(VenueSimulationTest, YieldBasedOrders) {
    // Create yield-based order
    TreasuryOrder order{};
    order.order_id = 1;
    order.instrument_type = TreasuryType::Note_10Y;
    order.order_type = OrderType::YieldLimit;
    order.side = OrderSide::Buy;
    order.yield_limit = 0.02;  // 2% yield
    order.quantity = 10;
    
    ASSERT_TRUE(venue_->submit_order(order));
    
    // Process acknowledgment
    VenueResponse response{};
    ASSERT_EQ(venue_->get_venue_responses(&response, 1), 1);
    EXPECT_EQ(response.new_status, OrderStatus::Acknowledged);
}

// Test high-throughput scenario
TEST_F(VenueSimulationTest, HighThroughput) {
    constexpr size_t ORDER_COUNT = 1000;
    
    // Submit many orders
    std::vector<TreasuryOrder> orders;
    for (size_t i = 0; i < ORDER_COUNT; ++i) {
        auto order = create_test_order(i + 1, 
            (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell,
            99.0 + (i % 10) * 0.1,
            10);
        ASSERT_TRUE(venue_->submit_order(order));
        orders.push_back(order);
    }
    
    // Process all responses
    size_t total_responses = 0;
    VenueResponse responses[256];
    while (true) {
        size_t count = venue_->get_venue_responses(responses, 256);
        if (count == 0) break;
        total_responses += count;
    }
    
    EXPECT_GE(total_responses, ORDER_COUNT);  // At least one response per order
    
    // Send market ticks
    for (int i = 0; i < 10; ++i) {
        auto tick = create_test_tick(
            99.0 + i * 0.1,
            99.25 + i * 0.1,
            100);
        venue_->process_market_update(tick);
    }
    
    // Process all fills
    size_t total_fills = 0;
    while (true) {
        size_t count = venue_->get_venue_responses(responses, 256);
        if (count == 0) break;
        total_fills += count;
    }
    
    EXPECT_GT(total_fills, 0);  // Should get some fills
    
    // Verify venue stats
    auto stats = venue_->get_venue_stats();
    EXPECT_EQ(stats.orders_submitted, ORDER_COUNT);
    EXPECT_GT(stats.orders_filled + stats.orders_partially_filled, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 