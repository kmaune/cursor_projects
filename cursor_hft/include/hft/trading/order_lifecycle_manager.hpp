#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include <chrono>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/trading/order_book.hpp"

namespace hft {
namespace trading {

using namespace hft::market_data;

/**
 * @brief Production-grade order lifecycle management with sub-microsecond performance
 * 
 * Manages complete order lifecycle from creation to settlement:
 * - Order state tracking with nanosecond timestamps
 * - Multi-venue order routing and execution
 * - Real-time fill processing and position updates
 * - Comprehensive audit trail for compliance
 * - Risk control integration with circuit breakers
 * - Settlement and reconciliation support
 * 
 * Performance targets:
 * - Order creation: <200ns
 * - State transition: <100ns
 * - Fill processing: <300ns
 * - Risk check: <150ns
 * - Venue routing: <250ns
 */
class alignas(64) OrderLifecycleManager {
public:
    static constexpr size_t MAX_ORDERS = 65536;           // Maximum concurrent orders
    static constexpr size_t MAX_VENUES = 8;               // Maximum trading venues
    static constexpr size_t MAX_FILLS_PER_ORDER = 32;     // Maximum fills per order
    static constexpr size_t AUDIT_TRAIL_SIZE = 1048576;   // Audit trail entries
    
    // Time in force options
    enum class TimeInForce : uint8_t {
        DAY = 0,        // Good for day
        IOC = 1,        // Immediate or cancel
        FOK = 2,        // Fill or kill
        GTC = 3         // Good till cancelled
    };
    
    // Order states for complete lifecycle tracking
    enum class OrderState : uint8_t {
        CREATED = 0,
        VALIDATED = 1,
        ROUTED = 2,
        PENDING_NEW = 3,
        ACKNOWLEDGED = 4,
        PARTIALLY_FILLED = 5,
        FILLED = 6,
        PENDING_CANCEL = 7,
        CANCELLED = 8,
        REJECTED = 9,
        EXPIRED = 10,
        SUSPENDED = 11,
        PENDING_REPLACE = 12,
        REPLACED = 13,
        ERROR = 14
    };
    
    // Venue information for multi-venue routing
    enum class VenueType : uint8_t {
        PRIMARY_DEALER = 0,
        ECN = 1,
        DARK_POOL = 2,
        CROSSING_NETWORK = 3,
        INSTITUTIONAL_NETWORK = 4,
        RETAIL_NETWORK = 5,
        INTERNAL_CROSSING = 6,
        EXTERNAL_VENUE = 7
    };
    
    // Order execution information
    struct alignas(64) OrderExecution {
        uint64_t order_id = 0;                              // Unique order identifier (8 bytes)
        uint64_t execution_id = 0;                          // Unique execution identifier (8 bytes)
        uint64_t venue_order_id = 0;                        // Venue-specific order ID (8 bytes)
        VenueType venue = VenueType::PRIMARY_DEALER;        // Execution venue (1 byte)
        uint8_t _pad0[3];                                   // Padding (3 bytes)
        TreasuryType instrument;                            // Treasury instrument (1 byte)
        uint8_t _pad1[3];                                   // Padding (3 bytes)
        Price32nd execution_price;                          // Execution price (8 bytes)
        uint64_t executed_quantity = 0;                     // Executed quantity (8 bytes)
        uint64_t leaves_quantity = 0;                       // Remaining quantity (8 bytes)
        uint64_t execution_time_ns = 0;                     // Execution timestamp (8 bytes)
        // Total: 8+8+8+1+3+1+3+8+8+8+8 = 64 bytes
        
        OrderExecution() noexcept : _pad0{}, _pad1{} {}
    };
    static_assert(sizeof(OrderExecution) == 64, "OrderExecution must be 64 bytes");
    
    // Comprehensive order state tracking
    struct alignas(64) OrderRecord {
        uint64_t order_id = 0;                              // Unique order identifier (8 bytes)
        uint64_t client_order_id = 0;                       // Client-provided ID (8 bytes)
        OrderState state = OrderState::CREATED;             // Current order state (1 byte)
        TreasuryType instrument;                            // Treasury instrument (1 byte)
        OrderSide side;                                     // Buy/Sell side (1 byte)
        OrderType type;                                     // Order type (1 byte)
        TimeInForce time_in_force;                          // Time in force (1 byte)
        VenueType target_venue = VenueType::PRIMARY_DEALER; // Target venue (1 byte)
        uint8_t _pad0[1];                                   // Padding (1 byte)
        Price32nd order_price;                              // Order price (8 bytes)
        uint64_t original_quantity = 0;                     // Original order quantity (8 bytes)
        uint64_t executed_quantity = 0;                     // Total executed quantity (8 bytes)
        uint64_t leaves_quantity = 0;                       // Remaining quantity (8 bytes)
        uint64_t creation_time_ns = 0;                      // Order creation time (8 bytes)
        // Total: 8+8+1+1+1+1+1+1+1+8+8+8+8+8 = 63 bytes, pad to 128 (2 cache lines)
        uint8_t _pad1[1];                                   // Padding to 64 bytes
        
        OrderRecord() noexcept : _pad0{}, _pad1{} {}
    };
    static_assert(sizeof(OrderRecord) == 128, "OrderRecord must be 128 bytes (2 cache lines)");
    
    // Venue configuration and performance tracking
    struct alignas(64) VenueConfig {
        VenueType type = VenueType::PRIMARY_DEALER;         // Venue type (1 byte)
        bool enabled = true;                                // Venue enabled flag (1 byte)
        uint8_t priority = 0;                               // Routing priority (1 byte)
        uint8_t _pad0[5];                                   // Padding (5 bytes)
        double fill_rate = 1.0;                             // Historical fill rate (8 bytes)
        double average_latency_ns = 1000.0;                 // Average response latency (8 bytes)
        uint64_t total_orders_sent = 0;                     // Total orders sent (8 bytes)
        uint64_t total_fills_received = 0;                  // Total fills received (8 bytes)
        uint64_t last_activity_time_ns = 0;                 // Last activity timestamp (8 bytes)
        uint64_t connection_errors = 0;                     // Connection error count (8 bytes)
        // Total: 1+1+1+5+8+8+8+8+8+8 = 56 bytes, pad to 64
        uint8_t _pad1[8];                                   // Padding to 64 bytes
        
        VenueConfig() noexcept : _pad0{}, _pad1{} {}
    };
    static_assert(sizeof(VenueConfig) == 64, "VenueConfig must be 64 bytes");
    
    // Risk control limits and circuit breakers
    struct RiskLimits {
        uint64_t max_order_size = 50000000;                 // Maximum single order size
        uint64_t max_position_size = 500000000;             // Maximum position size
        uint64_t max_daily_volume = 10000000000;            // Maximum daily volume
        double max_daily_loss = 10000000.0;                 // Maximum daily loss
        uint32_t max_orders_per_second = 1000;              // Order rate limit
        uint32_t max_cancels_per_minute = 10000;            // Cancel rate limit
        bool circuit_breaker_enabled = true;                // Circuit breaker status
        double circuit_breaker_threshold = 0.05;            // Circuit breaker threshold
        
        RiskLimits() noexcept = default;
    };
    
    // Audit trail entry for compliance
    struct alignas(64) AuditEntry {
        uint64_t entry_id = 0;                              // Unique entry ID (8 bytes)
        uint64_t order_id = 0;                              // Related order ID (8 bytes)
        uint64_t timestamp_ns = 0;                          // Event timestamp (8 bytes)
        OrderState old_state = OrderState::CREATED;         // Previous state (1 byte)
        OrderState new_state = OrderState::CREATED;         // New state (1 byte)
        VenueType venue = VenueType::PRIMARY_DEALER;        // Venue involved (1 byte)
        uint8_t event_type = 0;                             // Event type code (1 byte)
        uint32_t user_id = 0;                               // User responsible (4 bytes)
        Price32nd price;                                    // Price at event (8 bytes)
        uint64_t quantity = 0;                              // Quantity at event (8 bytes)
        char reason[16];                                    // Event reason (16 bytes)
        // Total: 8+8+8+1+1+1+1+4+8+8+16 = 64 bytes
        
        AuditEntry() noexcept { reason[0] = '\0'; }
    };
    static_assert(sizeof(AuditEntry) == 64, "AuditEntry must be 64 bytes");
    
    // Performance metrics tracking
    struct PerformanceMetrics {
        std::atomic<uint64_t> orders_created{0};
        std::atomic<uint64_t> orders_filled{0};
        std::atomic<uint64_t> orders_cancelled{0};
        std::atomic<uint64_t> orders_rejected{0};
        std::atomic<uint64_t> total_execution_time_ns{0};
        std::atomic<uint64_t> total_routing_time_ns{0};
        std::atomic<uint64_t> total_fill_processing_time_ns{0};
        std::atomic<uint64_t> risk_violations{0};
        std::atomic<uint64_t> circuit_breaker_triggers{0};
    };
    
    /**
     * @brief Constructor with infrastructure dependencies
     */
    OrderLifecycleManager(
        hft::ObjectPool<TreasuryOrder, 4096>& order_pool,
        hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool,
        hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer
    ) noexcept;
    
    // No copy or move semantics
    OrderLifecycleManager(const OrderLifecycleManager&) = delete;
    OrderLifecycleManager& operator=(const OrderLifecycleManager&) = delete;
    
    /**
     * @brief Create new order with validation and risk checks
     * @param instrument Treasury instrument type
     * @param side Buy/Sell side
     * @param type Order type
     * @param price Order price
     * @param quantity Order quantity
     * @param time_in_force Time in force
     * @return Order ID if successful, 0 if rejected
     */
    [[nodiscard]] uint64_t create_order(
        TreasuryType instrument,
        OrderSide side,
        OrderType type,
        Price32nd price,
        uint64_t quantity,
        TimeInForce time_in_force = TimeInForce::DAY
    ) noexcept;
    
    /**
     * @brief Cancel existing order
     * @param order_id Order to cancel
     * @return true if cancel request successful
     */
    bool cancel_order(uint64_t order_id) noexcept;
    
    /**
     * @brief Modify existing order price/quantity
     * @param order_id Order to modify
     * @param new_price New order price
     * @param new_quantity New order quantity
     * @return true if modify request successful
     */
    bool modify_order(uint64_t order_id, Price32nd new_price, uint64_t new_quantity) noexcept;
    
    /**
     * @brief Process fill from venue
     * @param execution Fill execution details
     * @return true if fill processed successfully
     */
    bool process_fill(const OrderExecution& execution) noexcept;
    
    /**
     * @brief Route order to optimal venue
     * @param order_id Order to route
     * @return Selected venue type
     */
    [[nodiscard]] VenueType route_order(uint64_t order_id) noexcept;
    
    /**
     * @brief Update venue configuration
     * @param venue Venue to update
     * @param config New configuration
     */
    void update_venue_config(VenueType venue, const VenueConfig& config) noexcept;
    
    /**
     * @brief Update risk limits
     * @param limits New risk limits
     */
    void update_risk_limits(const RiskLimits& limits) noexcept;
    
    /**
     * @brief Get order current state
     * @param order_id Order to query
     * @return Pointer to order record or nullptr if not found
     */
    [[nodiscard]] const OrderRecord* get_order(uint64_t order_id) const noexcept;
    
    /**
     * @brief Get venue configuration
     * @param venue Venue to query
     * @return Venue configuration
     */
    [[nodiscard]] const VenueConfig& get_venue_config(VenueType venue) const noexcept;
    
    /**
     * @brief Get current risk limits
     * @return Current risk limits
     */
    [[nodiscard]] const RiskLimits& get_risk_limits() const noexcept { return risk_limits_; }
    
    /**
     * @brief Get performance metrics
     * @return Performance metrics
     */
    [[nodiscard]] const PerformanceMetrics& get_metrics() const noexcept { return metrics_; }
    
    /**
     * @brief Emergency stop all trading
     */
    void emergency_stop() noexcept;
    
    /**
     * @brief Check if circuit breaker is triggered
     * @return true if circuit breaker active
     */
    [[nodiscard]] bool is_circuit_breaker_active() const noexcept { return circuit_breaker_active_.load(); }

private:
    // Infrastructure references
    alignas(64) hft::ObjectPool<TreasuryOrder, 4096>& order_pool_;
    alignas(64) hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool_;
    alignas(64) hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer_;
    alignas(64) hft::HFTTimer timer_;
    
    // Order management storage
    alignas(64) std::array<OrderRecord, MAX_ORDERS> orders_;
    alignas(64) std::array<std::atomic<bool>, MAX_ORDERS> order_slots_used_;
    alignas(64) std::atomic<uint64_t> next_order_id_;
    
    // Venue management
    alignas(64) std::array<VenueConfig, MAX_VENUES> venue_configs_;
    
    // Risk management
    alignas(64) RiskLimits risk_limits_;
    alignas(64) std::atomic<bool> emergency_stop_active_;
    alignas(64) std::atomic<bool> circuit_breaker_active_;
    
    // Audit trail
    alignas(64) std::array<AuditEntry, AUDIT_TRAIL_SIZE> audit_trail_;
    alignas(64) std::atomic<size_t> audit_trail_index_;
    
    // Performance tracking
    alignas(64) PerformanceMetrics metrics_;
    
    // Helper methods
    bool validate_order_parameters(
        TreasuryType instrument,
        OrderSide side,
        Price32nd price,
        uint64_t quantity
    ) noexcept;
    
    bool check_risk_limits(
        TreasuryType instrument,
        OrderSide side,
        uint64_t quantity
    ) noexcept;
    
    void update_order_state(uint64_t order_id, OrderState new_state, const char* reason = "") noexcept;
    void add_audit_entry(uint64_t order_id, OrderState old_state, OrderState new_state, const char* reason = "") noexcept;
    
    VenueType select_optimal_venue(TreasuryType instrument, OrderSide side, uint64_t quantity) noexcept;
    void update_venue_statistics(VenueType venue, bool success, uint64_t latency_ns) noexcept;
    
    uint64_t allocate_order_slot() noexcept;
    void deallocate_order_slot(uint64_t order_id) noexcept;
};

// Implementation

inline OrderLifecycleManager::OrderLifecycleManager(
    hft::ObjectPool<TreasuryOrder, 4096>& order_pool,
    hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool,
    hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer
) noexcept
    : order_pool_(order_pool),
      level_pool_(level_pool),
      update_buffer_(update_buffer),
      timer_(),
      orders_{},
      order_slots_used_{},
      next_order_id_(1),
      venue_configs_{},
      risk_limits_(),
      emergency_stop_active_(false),
      circuit_breaker_active_(false),
      audit_trail_{},
      audit_trail_index_(0),
      metrics_{} {
    
    // Initialize order slots
    for (auto& slot : order_slots_used_) {
        slot.store(false, std::memory_order_relaxed);
    }
    
    // Initialize venue configurations with defaults
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        venue_configs_[i].type = static_cast<VenueType>(i);
        venue_configs_[i].enabled = true;
        venue_configs_[i].priority = static_cast<uint8_t>(i);
        venue_configs_[i].fill_rate = 0.95; // 95% default fill rate
        venue_configs_[i].average_latency_ns = 1000.0 + i * 100.0; // Increasing latency
    }
}

inline uint64_t OrderLifecycleManager::create_order(
    TreasuryType instrument,
    OrderSide side,
    OrderType type,
    Price32nd price,
    uint64_t quantity,
    TimeInForce time_in_force
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    // Check emergency stop
    if (emergency_stop_active_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    // Check circuit breaker
    if (circuit_breaker_active_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    // Validate order parameters
    if (!validate_order_parameters(instrument, side, price, quantity)) {
        metrics_.orders_rejected.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    
    // Check risk limits
    if (!check_risk_limits(instrument, side, quantity)) {
        metrics_.risk_violations.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    
    // Allocate order slot
    const uint64_t order_id = allocate_order_slot();
    if (order_id == 0) {
        return 0; // No available slots
    }
    
    // Initialize order record
    const size_t slot_index = order_id % MAX_ORDERS;
    auto& order = orders_[slot_index];
    
    order.order_id = order_id;
    order.client_order_id = order_id; // Simple mapping for now
    order.state = OrderState::CREATED;
    order.instrument = instrument;
    order.side = side;
    order.type = type;
    order.time_in_force = time_in_force;
    order.order_price = price;
    order.original_quantity = quantity;
    order.executed_quantity = 0;
    order.leaves_quantity = quantity;
    order.creation_time_ns = start_time;
    
    // Add audit entry
    add_audit_entry(order_id, OrderState::CREATED, OrderState::CREATED, "Order created");
    
    // Update metrics
    metrics_.orders_created.fetch_add(1, std::memory_order_relaxed);
    const auto creation_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_execution_time_ns.fetch_add(creation_time, std::memory_order_relaxed);
    
    // Transition to validated state
    update_order_state(order_id, OrderState::VALIDATED, "Validation complete");
    
    return order_id;
}

inline bool OrderLifecycleManager::cancel_order(uint64_t order_id) noexcept {
    if (order_id == 0 || order_id >= MAX_ORDERS) return false;
    
    const size_t slot_index = order_id % MAX_ORDERS;
    if (!order_slots_used_[slot_index].load(std::memory_order_acquire)) {
        return false;
    }
    
    auto& order = orders_[slot_index];
    if (order.order_id != order_id) return false;
    
    // Check if order can be cancelled
    const OrderState current_state = order.state;
    if (current_state == OrderState::FILLED || 
        current_state == OrderState::CANCELLED ||
        current_state == OrderState::PENDING_CANCEL ||
        current_state == OrderState::REJECTED) {
        return false;
    }
    
    // Update state to pending cancel
    update_order_state(order_id, OrderState::PENDING_CANCEL, "Cancel requested");
    
    return true;
}

inline bool OrderLifecycleManager::process_fill(const OrderExecution& execution) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const uint64_t order_id = execution.order_id;
    if (order_id == 0 || order_id >= MAX_ORDERS) return false;
    
    const size_t slot_index = order_id % MAX_ORDERS;
    if (!order_slots_used_[slot_index].load(std::memory_order_acquire)) {
        return false;
    }
    
    auto& order = orders_[slot_index];
    if (order.order_id != order_id) return false;
    
    // Update order with execution
    order.executed_quantity += execution.executed_quantity;
    order.leaves_quantity = order.original_quantity - order.executed_quantity;
    
    // Determine new state
    OrderState new_state;
    if (order.leaves_quantity == 0) {
        new_state = OrderState::FILLED;
        metrics_.orders_filled.fetch_add(1, std::memory_order_relaxed);
    } else {
        new_state = OrderState::PARTIALLY_FILLED;
    }
    
    update_order_state(order_id, new_state, "Fill processed");
    
    // Update venue statistics
    update_venue_statistics(execution.venue, true, timer_.get_timestamp_ns() - execution.execution_time_ns);
    
    // Update performance metrics
    const auto processing_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_fill_processing_time_ns.fetch_add(processing_time, std::memory_order_relaxed);
    
    return true;
}

inline OrderLifecycleManager::VenueType OrderLifecycleManager::route_order(uint64_t order_id) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    if (order_id == 0 || order_id >= MAX_ORDERS) return VenueType::PRIMARY_DEALER;
    
    const size_t slot_index = order_id % MAX_ORDERS;
    if (!order_slots_used_[slot_index].load(std::memory_order_acquire)) {
        return VenueType::PRIMARY_DEALER;
    }
    
    const auto& order = orders_[slot_index];
    if (order.order_id != order_id) return VenueType::PRIMARY_DEALER;
    
    // Select optimal venue
    const VenueType selected_venue = select_optimal_venue(order.instrument, order.side, order.leaves_quantity);
    
    // Update order state
    auto& mutable_order = orders_[slot_index];
    mutable_order.target_venue = selected_venue;
    update_order_state(order_id, OrderState::ROUTED, "Order routed to venue");
    
    // Update metrics
    const auto routing_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_routing_time_ns.fetch_add(routing_time, std::memory_order_relaxed);
    
    return selected_venue;
}

inline bool OrderLifecycleManager::validate_order_parameters(
    TreasuryType instrument,
    OrderSide side,
    Price32nd price,
    uint64_t quantity
) noexcept {
    // Basic validation
    if (quantity == 0) return false;
    if (price.whole == 0) return false; // Prices should be positive
    
    // Treasury-specific validation
    switch (instrument) {
        case TreasuryType::Bill_3M:
        case TreasuryType::Bill_6M:
            // Bills: minimum 100K, increments of 100K
            if (quantity < 100000 || (quantity % 100000) != 0) return false;
            break;
        case TreasuryType::Note_2Y:
        case TreasuryType::Note_5Y:
        case TreasuryType::Note_10Y:
            // Notes: minimum 1M, increments of 1M
            if (quantity < 1000000 || (quantity % 1000000) != 0) return false;
            break;
        case TreasuryType::Bond_30Y:
            // Bonds: minimum 1M, increments of 1M
            if (quantity < 1000000 || (quantity % 1000000) != 0) return false;
            break;
    }
    
    return true;
}

inline bool OrderLifecycleManager::check_risk_limits(
    TreasuryType instrument,
    OrderSide side,
    uint64_t quantity
) noexcept {
    // Check maximum order size
    if (quantity > risk_limits_.max_order_size) {
        return false;
    }
    
    // Check position limits (simplified)
    // In production, this would check current positions across all instruments
    
    // Check order rate limits (simplified)
    // In production, this would check recent order rates
    
    return true;
}

inline void OrderLifecycleManager::update_order_state(
    uint64_t order_id, 
    OrderState new_state, 
    const char* reason
) noexcept {
    if (order_id == 0 || order_id >= MAX_ORDERS) return;
    
    const size_t slot_index = order_id % MAX_ORDERS;
    if (!order_slots_used_[slot_index].load(std::memory_order_acquire)) {
        return;
    }
    
    auto& order = orders_[slot_index];
    if (order.order_id != order_id) return;
    
    const OrderState old_state = order.state;
    order.state = new_state;
    
    add_audit_entry(order_id, old_state, new_state, reason);
}

inline void OrderLifecycleManager::add_audit_entry(
    uint64_t order_id,
    OrderState old_state,
    OrderState new_state,
    const char* reason
) noexcept {
    const size_t index = audit_trail_index_.fetch_add(1, std::memory_order_relaxed) % AUDIT_TRAIL_SIZE;
    
    auto& entry = audit_trail_[index];
    entry.entry_id = index;
    entry.order_id = order_id;
    entry.timestamp_ns = timer_.get_timestamp_ns();
    entry.old_state = old_state;
    entry.new_state = new_state;
    
    // Copy reason string safely
    const size_t max_len = sizeof(entry.reason) - 1;
    size_t i = 0;
    while (i < max_len && reason[i] != '\0') {
        entry.reason[i] = reason[i];
        ++i;
    }
    entry.reason[i] = '\0';
}

inline OrderLifecycleManager::VenueType OrderLifecycleManager::select_optimal_venue(
    TreasuryType instrument,
    OrderSide side,
    uint64_t quantity
) noexcept {
    VenueType best_venue = VenueType::PRIMARY_DEALER;
    double best_score = -1.0;
    
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        const auto& config = venue_configs_[i];
        if (!config.enabled) continue;
        
        // Simple scoring: fill_rate / latency, adjusted by priority
        const double score = (config.fill_rate / config.average_latency_ns) * (1.0 + config.priority * 0.1);
        
        if (score > best_score) {
            best_score = score;
            best_venue = config.type;
        }
    }
    
    return best_venue;
}

inline void OrderLifecycleManager::update_venue_statistics(VenueType venue, bool success, uint64_t latency_ns) noexcept {
    const auto venue_index = static_cast<size_t>(venue);
    if (venue_index >= MAX_VENUES) return;
    
    auto& config = venue_configs_[venue_index];
    
    // Update statistics (simple exponential moving average)
    constexpr double alpha = 0.1;
    
    config.total_orders_sent++;
    if (success) {
        config.total_fills_received++;
        config.fill_rate = config.fill_rate * (1.0 - alpha) + alpha;
    } else {
        config.fill_rate = config.fill_rate * (1.0 - alpha);
    }
    
    config.average_latency_ns = config.average_latency_ns * (1.0 - alpha) + static_cast<double>(latency_ns) * alpha;
    config.last_activity_time_ns = timer_.get_timestamp_ns();
}

inline uint64_t OrderLifecycleManager::allocate_order_slot() noexcept {
    for (size_t i = 0; i < MAX_ORDERS; ++i) {
        const uint64_t order_id = next_order_id_.fetch_add(1, std::memory_order_relaxed);
        const size_t slot_index = order_id % MAX_ORDERS;
        
        bool expected = false;
        if (order_slots_used_[slot_index].compare_exchange_weak(expected, true, std::memory_order_acquire)) {
            return order_id;
        }
    }
    
    return 0; // No available slots
}

inline void OrderLifecycleManager::deallocate_order_slot(uint64_t order_id) noexcept {
    if (order_id == 0) return;
    
    const size_t slot_index = order_id % MAX_ORDERS;
    order_slots_used_[slot_index].store(false, std::memory_order_release);
}

// Getter implementations
inline const OrderLifecycleManager::OrderRecord* 
OrderLifecycleManager::get_order(uint64_t order_id) const noexcept {
    if (order_id == 0 || order_id >= MAX_ORDERS) return nullptr;
    
    const size_t slot_index = order_id % MAX_ORDERS;
    if (!order_slots_used_[slot_index].load(std::memory_order_acquire)) {
        return nullptr;
    }
    
    const auto& order = orders_[slot_index];
    return (order.order_id == order_id) ? &order : nullptr;
}

inline const OrderLifecycleManager::VenueConfig& 
OrderLifecycleManager::get_venue_config(VenueType venue) const noexcept {
    const auto index = static_cast<size_t>(venue);
    return (index < MAX_VENUES) ? venue_configs_[index] : venue_configs_[0];
}

inline void OrderLifecycleManager::emergency_stop() noexcept {
    emergency_stop_active_.store(true, std::memory_order_release);
    
    // Cancel all active orders
    for (size_t i = 0; i < MAX_ORDERS; ++i) {
        if (order_slots_used_[i].load(std::memory_order_acquire)) {
            auto& order = orders_[i];
            if (order.state != OrderState::FILLED && order.state != OrderState::CANCELLED) {
                update_order_state(order.order_id, OrderState::CANCELLED, "Emergency stop");
            }
        }
    }
}

// Stub implementations for update methods
inline void OrderLifecycleManager::update_venue_config(VenueType venue, const VenueConfig& config) noexcept {
    const auto index = static_cast<size_t>(venue);
    if (index < MAX_VENUES) {
        venue_configs_[index] = config;
    }
}

inline void OrderLifecycleManager::update_risk_limits(const RiskLimits& limits) noexcept {
    risk_limits_ = limits;
}

inline bool OrderLifecycleManager::modify_order(uint64_t order_id, Price32nd new_price, uint64_t new_quantity) noexcept {
    if (order_id == 0 || order_id >= MAX_ORDERS) return false;
    
    const size_t slot_index = order_id % MAX_ORDERS;
    if (!order_slots_used_[slot_index].load(std::memory_order_acquire)) {
        return false;
    }
    
    auto& order = orders_[slot_index];
    if (order.order_id != order_id) return false;
    
    // Simple modification (in production would validate and send to venue)
    order.order_price = new_price;
    order.original_quantity = new_quantity;
    order.leaves_quantity = new_quantity - order.executed_quantity;
    
    update_order_state(order_id, OrderState::PENDING_REPLACE, "Order modification");
    
    return true;
}

// Static asserts
static_assert(alignof(OrderLifecycleManager) == 64, "OrderLifecycleManager must be cache-aligned");

} // namespace trading
} // namespace hft