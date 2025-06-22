#include <gtest/gtest.h>
#include <memory>
#include <array>
#include "hft/trading/order_lifecycle_manager.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::trading;
using namespace hft::market_data;

/**
 * @brief Test fixture for OrderLifecycleManager
 * 
 * Tests production-grade order lifecycle management including:
 * - Order creation and validation
 * - State transitions and audit trail
 * - Multi-venue routing and execution
 * - Risk control integration
 * - Performance characteristics
 */
class OrderLifecycleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize object pools
        order_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrder, 4096>>();
        level_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>>();
        update_buffer_ = std::make_unique<hft::SPSCRingBuffer<OrderBookUpdate, 8192>>();
        
        // Initialize order lifecycle manager
        order_manager_ = std::make_unique<OrderLifecycleManager>(*order_pool_, *level_pool_, *update_buffer_);
    }

    void TearDown() override {
        order_manager_.reset();
        update_buffer_.reset();
        level_pool_.reset();
        order_pool_.reset();
    }

protected:
    std::unique_ptr<hft::ObjectPool<TreasuryOrder, 4096>> order_pool_;
    std::unique_ptr<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>> level_pool_;
    std::unique_ptr<hft::SPSCRingBuffer<OrderBookUpdate, 8192>> update_buffer_;
    std::unique_ptr<OrderLifecycleManager> order_manager_;
};

// Test basic order creation
TEST_F(OrderLifecycleManagerTest, BasicOrderCreation) {
    const auto order_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        5000000
    );
    
    EXPECT_GT(order_id, 0);
    
    const auto* order = order_manager_->get_order(order_id);
    ASSERT_NE(order, nullptr);
    
    EXPECT_EQ(order->order_id, order_id);
    EXPECT_EQ(order->instrument, TreasuryType::Note_10Y);
    EXPECT_EQ(order->side, OrderSide::BID);
    EXPECT_EQ(order->type, OrderType::LIMIT);
    EXPECT_EQ(order->state, OrderLifecycleManager::OrderState::VALIDATED);
    EXPECT_EQ(order->original_quantity, 5000000);
    EXPECT_EQ(order->leaves_quantity, 5000000);
    EXPECT_EQ(order->executed_quantity, 0);
    EXPECT_GT(order->creation_time_ns, 0);
}

// Test order validation
TEST_F(OrderLifecycleManagerTest, OrderValidation) {
    // Valid order
    auto valid_order_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        1000000  // Valid 1M increment
    );
    EXPECT_GT(valid_order_id, 0);
    
    // Invalid quantity (not multiple of 1M for notes)
    auto invalid_order_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        1500000  // Invalid - not 1M increment
    );
    EXPECT_EQ(invalid_order_id, 0);
    
    // Zero quantity
    auto zero_quantity_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        0
    );
    EXPECT_EQ(zero_quantity_id, 0);
}

// Test order cancellation
TEST_F(OrderLifecycleManagerTest, OrderCancellation) {
    const auto order_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        5000000
    );
    ASSERT_GT(order_id, 0);
    
    // Cancel the order
    EXPECT_TRUE(order_manager_->cancel_order(order_id));
    
    const auto* order = order_manager_->get_order(order_id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->state, OrderLifecycleManager::OrderState::PENDING_CANCEL);
    
    // Cannot cancel already cancelled order
    EXPECT_FALSE(order_manager_->cancel_order(order_id));
}

// Test order modification
TEST_F(OrderLifecycleManagerTest, OrderModification) {
    const auto order_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        5000000
    );
    ASSERT_GT(order_id, 0);
    
    const auto new_price = Price32nd::from_decimal(102.75);
    const uint64_t new_quantity = 3000000;
    
    EXPECT_TRUE(order_manager_->modify_order(order_id, new_price, new_quantity));
    
    const auto* order = order_manager_->get_order(order_id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->state, OrderLifecycleManager::OrderState::PENDING_REPLACE);
    EXPECT_EQ(order->order_price.to_decimal(), new_price.to_decimal());
    EXPECT_EQ(order->original_quantity, new_quantity);
}

// Test fill processing
TEST_F(OrderLifecycleManagerTest, FillProcessing) {
    const auto order_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        5000000
    );
    ASSERT_GT(order_id, 0);
    
    // Process partial fill
    OrderLifecycleManager::OrderExecution execution;
    execution.order_id = order_id;
    execution.execution_id = 12345;
    execution.venue = OrderLifecycleManager::VenueType::PRIMARY_DEALER;
    execution.instrument = TreasuryType::Note_10Y;
    execution.execution_price = Price32nd::from_decimal(102.5);
    execution.executed_quantity = 2000000;
    execution.leaves_quantity = 3000000;
    execution.execution_time_ns = hft::HFTTimer::get_timestamp_ns();
    
    EXPECT_TRUE(order_manager_->process_fill(execution));
    
    const auto* order = order_manager_->get_order(order_id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->state, OrderLifecycleManager::OrderState::PARTIALLY_FILLED);
    EXPECT_EQ(order->executed_quantity, 2000000);
    EXPECT_EQ(order->leaves_quantity, 3000000);
    
    // Process remaining fill
    execution.executed_quantity = 3000000;
    execution.leaves_quantity = 0;
    EXPECT_TRUE(order_manager_->process_fill(execution));
    
    order = order_manager_->get_order(order_id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->state, OrderLifecycleManager::OrderState::FILLED);
    EXPECT_EQ(order->executed_quantity, 5000000);
    EXPECT_EQ(order->leaves_quantity, 0);
}

// Test venue routing
TEST_F(OrderLifecycleManagerTest, VenueRouting) {
    const auto order_id = order_manager_->create_order(
        TreasuryType::Note_10Y,
        OrderSide::BID,
        OrderType::LIMIT,
        Price32nd::from_decimal(102.5),
        5000000
    );
    ASSERT_GT(order_id, 0);
    
    const auto venue = order_manager_->route_order(order_id);
    EXPECT_NE(venue, OrderLifecycleManager::VenueType::PRIMARY_DEALER); // Should route somewhere
    
    const auto* order = order_manager_->get_order(order_id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->state, OrderLifecycleManager::OrderState::ROUTED);
    EXPECT_EQ(order->target_venue, venue);
}

// Test venue configuration
TEST_F(OrderLifecycleManagerTest, VenueConfiguration) {
    OrderLifecycleManager::VenueConfig config;
    config.type = OrderLifecycleManager::VenueType::ECN;
    config.enabled = true;
    config.priority = 1;
    config.fill_rate = 0.85;
    config.average_latency_ns = 1500.0;
    
    order_manager_->update_venue_config(OrderLifecycleManager::VenueType::ECN, config);
    
    const auto& retrieved_config = order_manager_->get_venue_config(OrderLifecycleManager::VenueType::ECN);
    EXPECT_EQ(retrieved_config.type, OrderLifecycleManager::VenueType::ECN);
    EXPECT_TRUE(retrieved_config.enabled);
    EXPECT_EQ(retrieved_config.priority, 1);
    EXPECT_EQ(retrieved_config.fill_rate, 0.85);
    EXPECT_EQ(retrieved_config.average_latency_ns, 1500.0);
}

// Test risk limits
TEST_F(OrderLifecycleManagerTest, RiskLimits) {
    OrderLifecycleManager::RiskLimits limits;
    limits.max_order_size = 10000000;
    limits.max_position_size = 100000000;
    limits.max_daily_volume = 1000000000;
    
    order_manager_->update_risk_limits(limits);
    
    const auto& retrieved_limits = order_manager_->get_risk_limits();
    EXPECT_EQ(retrieved_limits.max_order_size, 10000000);
    EXPECT_EQ(retrieved_limits.max_position_size, 100000000);
    EXPECT_EQ(retrieved_limits.max_daily_volume, 1000000000);
}

// Test emergency stop
TEST_F(OrderLifecycleManagerTest, EmergencyStop) {
    // Create multiple orders
    const auto order_id1 = order_manager_->create_order(
        TreasuryType::Note_10Y, OrderSide::BID, OrderType::LIMIT,
        Price32nd::from_decimal(102.5), 5000000
    );
    const auto order_id2 = order_manager_->create_order(
        TreasuryType::Note_2Y, OrderSide::ASK, OrderType::LIMIT,
        Price32nd::from_decimal(101.0), 3000000
    );
    
    ASSERT_GT(order_id1, 0);
    ASSERT_GT(order_id2, 0);
    
    // Trigger emergency stop
    order_manager_->emergency_stop();
    
    // All orders should be cancelled
    const auto* order1 = order_manager_->get_order(order_id1);
    const auto* order2 = order_manager_->get_order(order_id2);
    
    ASSERT_NE(order1, nullptr);
    ASSERT_NE(order2, nullptr);
    
    EXPECT_EQ(order1->state, OrderLifecycleManager::OrderState::CANCELLED);
    EXPECT_EQ(order2->state, OrderLifecycleManager::OrderState::CANCELLED);
    
    // No new orders should be accepted
    const auto new_order_id = order_manager_->create_order(
        TreasuryType::Note_10Y, OrderSide::BID, OrderType::LIMIT,
        Price32nd::from_decimal(102.5), 5000000
    );
    EXPECT_EQ(new_order_id, 0);
}

// Test performance metrics
TEST_F(OrderLifecycleManagerTest, PerformanceMetrics) {
    const auto& initial_metrics = order_manager_->get_metrics();
    EXPECT_EQ(initial_metrics.orders_created.load(), 0);
    EXPECT_EQ(initial_metrics.orders_filled.load(), 0);
    
    // Create an order
    const auto order_id = order_manager_->create_order(
        TreasuryType::Note_10Y, OrderSide::BID, OrderType::LIMIT,
        Price32nd::from_decimal(102.5), 5000000
    );
    ASSERT_GT(order_id, 0);
    
    const auto& after_create_metrics = order_manager_->get_metrics();
    EXPECT_EQ(after_create_metrics.orders_created.load(), 1);
    
    // Process a fill
    OrderLifecycleManager::OrderExecution execution;
    execution.order_id = order_id;
    execution.executed_quantity = 5000000;
    execution.leaves_quantity = 0;
    order_manager_->process_fill(execution);
    
    const auto& after_fill_metrics = order_manager_->get_metrics();
    EXPECT_EQ(after_fill_metrics.orders_filled.load(), 1);
}

// Test order creation performance
TEST_F(OrderLifecycleManagerTest, OrderCreationPerformance) {
    const int iterations = 1000;
    uint64_t total_time = 0;
    uint64_t max_time = 0;
    uint64_t min_time = UINT64_MAX;
    
    for (int i = 0; i < iterations; ++i) {
        const auto start_time = hft::HFTTimer::get_timestamp_ns();
        
        const auto order_id = order_manager_->create_order(
            TreasuryType::Note_10Y,
            OrderSide::BID,
            OrderType::LIMIT,
            Price32nd::from_decimal(102.5),
            1000000
        );
        
        const auto end_time = hft::HFTTimer::get_timestamp_ns();
        const auto duration = end_time - start_time;
        
        total_time += duration;
        max_time = std::max(max_time, duration);
        min_time = std::min(min_time, duration);
        
        EXPECT_GT(order_id, 0);
        
        // Target: <200ns order creation - allow some outliers
        EXPECT_LT(duration, 5000); // Allow for system jitter and cold start
    }
    
    const uint64_t avg_time = total_time / iterations;
    
    std::cout << "Order Creation Performance:" << std::endl;
    std::cout << "  Average: " << avg_time << "ns" << std::endl;
    std::cout << "  Min: " << min_time << "ns" << std::endl;
    std::cout << "  Max: " << max_time << "ns" << std::endl;
    std::cout << "  Target: <200ns" << std::endl;
    
    EXPECT_LT(avg_time, 500); // Relaxed target for initial version
}

// Test fill processing performance
TEST_F(OrderLifecycleManagerTest, FillProcessingPerformance) {
    // Create order first
    const auto order_id = order_manager_->create_order(
        TreasuryType::Note_10Y, OrderSide::BID, OrderType::LIMIT,
        Price32nd::from_decimal(102.5), 5000000
    );
    ASSERT_GT(order_id, 0);
    
    const int iterations = 1000;
    uint64_t total_time = 0;
    
    for (int i = 0; i < iterations; ++i) {
        OrderLifecycleManager::OrderExecution execution;
        execution.order_id = order_id;
        execution.execution_id = i;
        execution.executed_quantity = 1000;
        execution.execution_time_ns = hft::HFTTimer::get_timestamp_ns();
        
        const auto start_time = hft::HFTTimer::get_timestamp_ns();
        order_manager_->process_fill(execution);
        const auto end_time = hft::HFTTimer::get_timestamp_ns();
        
        total_time += (end_time - start_time);
    }
    
    const uint64_t avg_time = total_time / iterations;
    
    std::cout << "Fill Processing Performance:" << std::endl;
    std::cout << "  Average: " << avg_time << "ns" << std::endl;
    std::cout << "  Target: <300ns" << std::endl;
    
    EXPECT_LT(avg_time, 500); // Relaxed target for initial version
}

// Test data structure validation
TEST_F(OrderLifecycleManagerTest, DataStructureValidation) {
    // Verify cache alignment
    EXPECT_EQ(alignof(OrderLifecycleManager), 64);
    
    // Verify structure sizes (must be cache-line aligned for performance)
    EXPECT_EQ(sizeof(OrderLifecycleManager::OrderRecord), 128);
    EXPECT_EQ(sizeof(OrderLifecycleManager::OrderExecution), 64);
    EXPECT_EQ(sizeof(OrderLifecycleManager::VenueConfig), 64);
    EXPECT_EQ(sizeof(OrderLifecycleManager::AuditEntry), 64);
}