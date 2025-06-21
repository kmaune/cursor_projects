#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>
#include <random>
#include "hft/trading/order_book.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::trading;
using namespace hft::market_data;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pools and ring buffer
        order_pool_ = std::make_unique<TreasuryOrderPool>();
        level_pool_ = std::make_unique<PriceLevelPool>();
        update_buffer_ = std::make_unique<OrderBookUpdateBuffer>();
        
        // Create order book with pool references
        order_book_ = std::make_unique<TreasuryOrderBook>(
            *order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize sequence counter
        next_sequence_ = 1;
    }

    void TearDown() override {
        order_book_->reset();
        order_pool_->reset();
        level_pool_->reset();
    }

    // Helper function to create test orders
    TreasuryOrder create_test_order(uint64_t order_id, OrderSide side, 
                                   double price_decimal, uint64_t quantity) {
        Price32nd price = Price32nd::from_decimal(price_decimal);
        return TreasuryOrder(order_id, TreasuryType::Note_10Y, side, 
                           OrderType::LIMIT, price, quantity, next_sequence_++);
    }

    // Helper function to create price from decimal
    Price32nd make_price(double decimal) {
        return Price32nd::from_decimal(decimal);
    }

    std::unique_ptr<TreasuryOrderPool> order_pool_;
    std::unique_ptr<PriceLevelPool> level_pool_;
    std::unique_ptr<OrderBookUpdateBuffer> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    uint64_t next_sequence_;
};

// Basic functionality tests
TEST_F(OrderBookTest, EmptyBookState) {
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_bid_levels, 0);
    EXPECT_EQ(stats.total_ask_levels, 0);
    EXPECT_EQ(stats.total_orders, 0);
    
    auto best_bid = order_book_->get_best_bid();
    auto best_ask = order_book_->get_best_ask();
    
    EXPECT_EQ(best_bid.second, 0); // No quantity
    EXPECT_EQ(best_ask.second, 0); // No quantity
}

TEST_F(OrderBookTest, AddSingleBidOrder) {
    TreasuryOrder order = create_test_order(1, OrderSide::BID, 99.5, 1000000);
    
    EXPECT_TRUE(order_book_->add_order(order));
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_bid_levels, 1);
    EXPECT_EQ(stats.total_ask_levels, 0);
    EXPECT_EQ(stats.total_orders, 1);
    
    auto best_bid = order_book_->get_best_bid();
    EXPECT_EQ(best_bid.first.to_decimal(), 99.5);
    EXPECT_EQ(best_bid.second, 1000000);
}

TEST_F(OrderBookTest, AddSingleAskOrder) {
    TreasuryOrder order = create_test_order(1, OrderSide::ASK, 99.75, 2000000);
    
    EXPECT_TRUE(order_book_->add_order(order));
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_bid_levels, 0);
    EXPECT_EQ(stats.total_ask_levels, 1);
    EXPECT_EQ(stats.total_orders, 1);
    
    auto best_ask = order_book_->get_best_ask();
    EXPECT_EQ(best_ask.first.to_decimal(), 99.75);
    EXPECT_EQ(best_ask.second, 2000000);
}

TEST_F(OrderBookTest, MultipleBidLevels) {
    // Add bids in random order
    EXPECT_TRUE(order_book_->add_order(create_test_order(2, OrderSide::BID, 99.25, 500000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(1, OrderSide::BID, 99.5, 1000000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(3, OrderSide::BID, 99.0, 750000)));
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_bid_levels, 3);
    EXPECT_EQ(stats.total_orders, 3);
    
    // Best bid should be highest price (99.5)
    auto best_bid = order_book_->get_best_bid();
    EXPECT_EQ(best_bid.first.to_decimal(), 99.5);
    EXPECT_EQ(best_bid.second, 1000000);
    
    // Check market depth
    auto depth = order_book_->get_market_depth(OrderSide::BID, 5);
    ASSERT_EQ(depth.size(), 3);
    
    EXPECT_EQ(depth[0].price.to_decimal(), 99.5);
    EXPECT_EQ(depth[0].quantity, 1000000);
    EXPECT_EQ(depth[1].price.to_decimal(), 99.25);
    EXPECT_EQ(depth[1].quantity, 500000);
    EXPECT_EQ(depth[2].price.to_decimal(), 99.0);
    EXPECT_EQ(depth[2].quantity, 750000);
}

TEST_F(OrderBookTest, MultipleAskLevels) {
    // Add asks in random order
    EXPECT_TRUE(order_book_->add_order(create_test_order(2, OrderSide::ASK, 100.0, 800000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(1, OrderSide::ASK, 99.75, 1200000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(3, OrderSide::ASK, 100.25, 600000)));
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_ask_levels, 3);
    EXPECT_EQ(stats.total_orders, 3);
    
    // Best ask should be lowest price (99.75)
    auto best_ask = order_book_->get_best_ask();
    EXPECT_EQ(best_ask.first.to_decimal(), 99.75);
    EXPECT_EQ(best_ask.second, 1200000);
    
    // Check market depth
    auto depth = order_book_->get_market_depth(OrderSide::ASK, 5);
    ASSERT_EQ(depth.size(), 3);
    
    EXPECT_EQ(depth[0].price.to_decimal(), 99.75);
    EXPECT_EQ(depth[0].quantity, 1200000);
    EXPECT_EQ(depth[1].price.to_decimal(), 100.0);
    EXPECT_EQ(depth[1].quantity, 800000);
    EXPECT_EQ(depth[2].price.to_decimal(), 100.25);
    EXPECT_EQ(depth[2].quantity, 600000);
}

TEST_F(OrderBookTest, SamePriceLevelMultipleOrders) {
    // Add multiple orders at same price level
    EXPECT_TRUE(order_book_->add_order(create_test_order(1, OrderSide::BID, 99.5, 500000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(2, OrderSide::BID, 99.5, 300000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(3, OrderSide::BID, 99.5, 200000)));
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_bid_levels, 1); // Only one price level
    EXPECT_EQ(stats.total_orders, 3);     // But three orders
    
    // Total quantity should be sum of all orders
    auto best_bid = order_book_->get_best_bid();
    EXPECT_EQ(best_bid.first.to_decimal(), 99.5);
    EXPECT_EQ(best_bid.second, 1000000); // 500k + 300k + 200k
    
    // Market depth should show aggregated quantity
    auto depth = order_book_->get_market_depth(OrderSide::BID, 1);
    ASSERT_EQ(depth.size(), 1);
    EXPECT_EQ(depth[0].quantity, 1000000);
    EXPECT_EQ(depth[0].order_count, 3);
}

// Order cancellation tests
TEST_F(OrderBookTest, CancelOrder) {
    TreasuryOrder order = create_test_order(1, OrderSide::BID, 99.5, 1000000);
    EXPECT_TRUE(order_book_->add_order(order));
    
    auto stats_before = order_book_->get_stats();
    EXPECT_EQ(stats_before.total_orders, 1);
    
    EXPECT_TRUE(order_book_->cancel_order(1));
    
    auto stats_after = order_book_->get_stats();
    EXPECT_EQ(stats_after.total_orders, 0);
    EXPECT_EQ(stats_after.total_bid_levels, 0);
    
    // Best bid should be empty
    auto best_bid = order_book_->get_best_bid();
    EXPECT_EQ(best_bid.second, 0);
}

TEST_F(OrderBookTest, CancelNonExistentOrder) {
    EXPECT_FALSE(order_book_->cancel_order(999)); // Order doesn't exist
}

TEST_F(OrderBookTest, CancelOneOfMultipleOrdersAtSameLevel) {
    // Add multiple orders at same level
    EXPECT_TRUE(order_book_->add_order(create_test_order(1, OrderSide::BID, 99.5, 500000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(2, OrderSide::BID, 99.5, 300000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(3, OrderSide::BID, 99.5, 200000)));
    
    // Cancel middle order
    EXPECT_TRUE(order_book_->cancel_order(2));
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_orders, 2);
    EXPECT_EQ(stats.total_bid_levels, 1); // Level still exists
    
    // Quantity should be reduced
    auto best_bid = order_book_->get_best_bid();
    EXPECT_EQ(best_bid.second, 700000); // 500k + 200k
}

// Order modification tests
TEST_F(OrderBookTest, ModifyOrderPrice) {
    TreasuryOrder order = create_test_order(1, OrderSide::BID, 99.5, 1000000);
    EXPECT_TRUE(order_book_->add_order(order));
    
    // Modify price (should be cancel + add)
    EXPECT_TRUE(order_book_->modify_order(1, make_price(99.75), 1000000));
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_orders, 1);
    
    auto best_bid = order_book_->get_best_bid();
    EXPECT_EQ(best_bid.first.to_decimal(), 99.75);
    EXPECT_EQ(best_bid.second, 1000000);
}

TEST_F(OrderBookTest, ModifyOrderQuantity) {
    TreasuryOrder order = create_test_order(1, OrderSide::ASK, 100.0, 1000000);
    EXPECT_TRUE(order_book_->add_order(order));
    
    // Modify quantity
    EXPECT_TRUE(order_book_->modify_order(1, make_price(100.0), 1500000));
    
    auto best_ask = order_book_->get_best_ask();
    EXPECT_EQ(best_ask.first.to_decimal(), 100.0);
    EXPECT_EQ(best_ask.second, 1500000);
}

TEST_F(OrderBookTest, ModifyNonExistentOrder) {
    EXPECT_FALSE(order_book_->modify_order(999, make_price(99.5), 1000000));
}

// Trade execution tests
TEST_F(OrderBookTest, ProcessTradeFullyFillsOrder) {
    TreasuryOrder order = create_test_order(1, OrderSide::ASK, 100.0, 1000000);
    EXPECT_TRUE(order_book_->add_order(order));
    
    // Execute trade that fully fills the order
    size_t orders_affected = order_book_->process_trade(make_price(100.0), 1000000, OrderSide::ASK);
    
    EXPECT_EQ(orders_affected, 1);
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_orders, 0); // Order should be removed
    EXPECT_EQ(stats.total_ask_levels, 0); // Level should be removed
}

TEST_F(OrderBookTest, ProcessTradePartiallyFillsOrder) {
    TreasuryOrder order = create_test_order(1, OrderSide::BID, 99.5, 1000000);
    EXPECT_TRUE(order_book_->add_order(order));
    
    // Execute trade that partially fills the order
    size_t orders_affected = order_book_->process_trade(make_price(99.5), 400000, OrderSide::BID);
    
    EXPECT_EQ(orders_affected, 1);
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_orders, 1); // Order still exists
    
    auto best_bid = order_book_->get_best_bid();
    EXPECT_EQ(best_bid.second, 600000); // Remaining quantity
}

TEST_F(OrderBookTest, ProcessTradeMultipleOrdersTimePriority) {
    // Add orders in chronological order (time priority)
    EXPECT_TRUE(order_book_->add_order(create_test_order(1, OrderSide::ASK, 100.0, 300000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(2, OrderSide::ASK, 100.0, 500000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(3, OrderSide::ASK, 100.0, 200000)));
    
    // Execute trade that affects multiple orders
    size_t orders_affected = order_book_->process_trade(make_price(100.0), 600000, OrderSide::ASK);
    
    EXPECT_EQ(orders_affected, 2); // First order fully filled, second partially
    
    auto stats = order_book_->get_stats();
    EXPECT_EQ(stats.total_orders, 2); // Two orders remain
    
    auto best_ask = order_book_->get_best_ask();
    EXPECT_EQ(best_ask.second, 400000); // 200k (partial) + 200k (full)
}

// Treasury-specific tests
TEST_F(OrderBookTest, Treasury32ndPricing) {
    // Test 32nd fractional pricing precision
    TreasuryOrder order1 = create_test_order(1, OrderSide::BID, 99.515625, 1000000); // 99-16.5
    TreasuryOrder order2 = create_test_order(2, OrderSide::BID, 99.53125, 1000000);  // 99-17
    
    EXPECT_TRUE(order_book_->add_order(order1));
    EXPECT_TRUE(order_book_->add_order(order2));
    
    // Higher price (99-17) should be best bid
    auto best_bid = order_book_->get_best_bid();
    EXPECT_DOUBLE_EQ(best_bid.first.to_decimal(), 99.53125);
    
    // Check depth ordering
    auto depth = order_book_->get_market_depth(OrderSide::BID, 2);
    ASSERT_EQ(depth.size(), 2);
    EXPECT_DOUBLE_EQ(depth[0].price.to_decimal(), 99.53125);
    EXPECT_DOUBLE_EQ(depth[1].price.to_decimal(), 99.515625);
}

// Edge case tests
TEST_F(OrderBookTest, InvalidOrderRejection) {
    // Test invalid orders are rejected
    TreasuryOrder invalid_order(0, TreasuryType::Note_10Y, OrderSide::BID, 
                               OrderType::LIMIT, make_price(99.5), 1000000, 1);
    EXPECT_FALSE(order_book_->add_order(invalid_order)); // order_id = 0
    
    TreasuryOrder zero_qty_order(1, TreasuryType::Note_10Y, OrderSide::BID, 
                                OrderType::LIMIT, make_price(99.5), 0, 1);
    EXPECT_FALSE(order_book_->add_order(zero_qty_order)); // qty = 0
}

TEST_F(OrderBookTest, DuplicateOrderIdRejection) {
    TreasuryOrder order1 = create_test_order(1, OrderSide::BID, 99.5, 1000000);
    TreasuryOrder order2 = create_test_order(1, OrderSide::ASK, 100.0, 1000000); // Same ID
    
    EXPECT_TRUE(order_book_->add_order(order1));
    EXPECT_FALSE(order_book_->add_order(order2)); // Duplicate ID should be rejected
}

TEST_F(OrderBookTest, BookReset) {
    // Add several orders
    EXPECT_TRUE(order_book_->add_order(create_test_order(1, OrderSide::BID, 99.5, 1000000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(2, OrderSide::ASK, 100.0, 1000000)));
    EXPECT_TRUE(order_book_->add_order(create_test_order(3, OrderSide::BID, 99.25, 500000)));
    
    auto stats_before = order_book_->get_stats();
    EXPECT_GT(stats_before.total_orders, 0);
    
    // Reset book
    order_book_->reset();
    
    auto stats_after = order_book_->get_stats();
    EXPECT_EQ(stats_after.total_orders, 0);
    EXPECT_EQ(stats_after.total_bid_levels, 0);
    EXPECT_EQ(stats_after.total_ask_levels, 0);
    
    // Best prices should be empty
    auto best_bid = order_book_->get_best_bid();
    auto best_ask = order_book_->get_best_ask();
    EXPECT_EQ(best_bid.second, 0);
    EXPECT_EQ(best_ask.second, 0);
}

// Performance tests
TEST_F(OrderBookTest, BasicLatencyMeasurement) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Add several orders and measure overall time
    for (int i = 1; i <= 100; ++i) {
        double price = 99.0 + (i % 20) * 0.03125; // Spread across price levels
        OrderSide side = (i % 2 == 0) ? OrderSide::BID : OrderSide::ASK;
        EXPECT_TRUE(order_book_->add_order(create_test_order(i, side, price, 1000000)));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    // Should average well under 500ns per operation
    double avg_ns = static_cast<double>(duration.count()) / 100.0;
    
    // This is a rough check - actual performance testing is in benchmarks
    EXPECT_LT(avg_ns, 2000.0) << "Average operation time too high: " << avg_ns << "ns";
    
    auto stats = order_book_->get_stats();
    EXPECT_GT(stats.avg_operation_latency_ns, 0);
}

// Object pool integration tests
TEST_F(OrderBookTest, ObjectPoolIntegration) {
    // Fill the book with orders to test pool usage
    size_t initial_available_orders = order_pool_->available();
    size_t initial_available_levels = level_pool_->available();
    
    // Add orders across multiple price levels
    std::vector<uint64_t> order_ids;
    for (int i = 1; i <= 50; ++i) {
        double price = 99.0 + (i * 0.03125);
        order_ids.push_back(i);
        EXPECT_TRUE(order_book_->add_order(create_test_order(i, OrderSide::BID, price, 1000000)));
    }
    
    // Check that pool objects were consumed
    EXPECT_LT(order_pool_->available(), initial_available_orders);
    EXPECT_LT(level_pool_->available(), initial_available_levels);
    
    // Cancel all orders
    for (uint64_t order_id : order_ids) {
        EXPECT_TRUE(order_book_->cancel_order(order_id));
    }
    
    // Objects should be returned to pools
    EXPECT_EQ(order_pool_->available(), initial_available_orders);
    EXPECT_EQ(level_pool_->available(), initial_available_levels);
}

// Ring buffer notification tests
TEST_F(OrderBookTest, OrderBookUpdateNotifications) {
    size_t initial_buffer_size = update_buffer_->size();
    
    // Add an order
    EXPECT_TRUE(order_book_->add_order(create_test_order(1, OrderSide::BID, 99.5, 1000000)));
    
    // Should have notification in buffer
    EXPECT_GT(update_buffer_->size(), initial_buffer_size);
    
    // Read the notification
    OrderBookUpdate update;
    EXPECT_TRUE(update_buffer_->try_pop(update));
    EXPECT_EQ(update.update_type, OrderBookUpdate::ORDER_ADDED);
    EXPECT_EQ(update.order_id, 1);
    EXPECT_EQ(update.side, OrderSide::BID);
    EXPECT_DOUBLE_EQ(update.price.to_decimal(), 99.5);
    EXPECT_EQ(update.quantity, 1000000);
}

// Stress test with random operations
TEST_F(OrderBookTest, RandomOperationsStressTest) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> operation_dist(0, 2); // 0=add, 1=cancel, 2=modify
    std::uniform_real_distribution<> price_dist(98.0, 102.0);
    std::uniform_int_distribution<uint64_t> qty_dist(100000, 5000000);
    std::uniform_int_distribution<> side_dist(0, 1);
    
    std::vector<uint64_t> active_orders;
    
    // Perform random operations
    for (int i = 0; i < 500; ++i) {
        int operation = operation_dist(gen);
        
        if (operation == 0 || active_orders.empty()) {
            // Add order
            uint64_t order_id = i + 1;
            OrderSide side = (side_dist(gen) == 0) ? OrderSide::BID : OrderSide::ASK;
            double price = price_dist(gen);
            uint64_t quantity = qty_dist(gen);
            
            if (order_book_->add_order(create_test_order(order_id, side, price, quantity))) {
                active_orders.push_back(order_id);
            }
        } else if (operation == 1 && !active_orders.empty()) {
            // Cancel order
            size_t idx = gen() % active_orders.size();
            uint64_t order_id = active_orders[idx];
            
            if (order_book_->cancel_order(order_id)) {
                active_orders.erase(active_orders.begin() + idx);
            }
        } else if (operation == 2 && !active_orders.empty()) {
            // Modify order
            size_t idx = gen() % active_orders.size();
            uint64_t order_id = active_orders[idx];
            double new_price = price_dist(gen);
            uint64_t new_quantity = qty_dist(gen);
            
            // Modify might fail if order was already cancelled
            (void)order_book_->modify_order(order_id, make_price(new_price), new_quantity);
        }
    }
    
    // Verify book is still in valid state
    auto stats = order_book_->get_stats();
    EXPECT_LE(stats.total_orders, active_orders.size());
    
    // Clean up
    order_book_->reset();
}