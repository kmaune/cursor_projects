#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <array>
#include <algorithm>
#include <string_view>
#include <random>
#include <atomic>
#include <utility>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

namespace hft {
namespace market_data {

// ========================= 1. Order and Venue Message Definitions =========================

enum class OrderType : uint8_t {
    Limit,              // Standard limit order
    YieldLimit,         // Yield-based limit order
    Market,             // Market order (rare in treasury)
    Cancel,             // Cancel existing order
    Invalid = 0xFF
};

enum class OrderSide : uint8_t { Buy, Sell };

enum class OrderStatus : uint8_t {
    New,                // Just created
    Submitted,          // Sent to venue
    Acknowledged,       // Venue confirmed receipt
    PartiallyFilled,   // Partial execution
    Filled,            // Fully executed
    Rejected,          // Venue rejected
    Cancelled,         // Successfully cancelled
    Expired            // Time-based expiration
};

// Cache-aligned order structure
struct alignas(CACHE_LINE_SIZE) TreasuryOrder {
    uint64_t order_id;                          // Unique order identifier
    uint64_t client_order_id;                   // Client-side tracking ID
    HFTTimer::timestamp_t timestamp_created_ns; // Order creation time
    HFTTimer::timestamp_t timestamp_venue_ns;   // Last venue interaction
    TreasuryType instrument_type;               // Which treasury instrument
    OrderType order_type;                       // Order type
    OrderSide side;                             // Buy/Sell
    OrderStatus status;                         // Current order status
    Price32nd limit_price;                      // Limit price in 32nds
    double yield_limit;                         // For yield-based orders (4 decimals)
    uint64_t quantity;                          // Order size in millions
    uint64_t filled_quantity;                   // Already filled amount
    uint64_t remaining_quantity;                // Unfilled amount
    char venue_order_id[16];                    // Venue-assigned ID
    uint8_t _pad[8];                           // Pad to 64 bytes
};
static_assert(sizeof(TreasuryOrder) == CACHE_LINE_SIZE, "TreasuryOrder must be 64 bytes");

// Venue response messages
struct alignas(CACHE_LINE_SIZE) VenueResponse {
    uint64_t order_id;                          // References TreasuryOrder
    HFTTimer::timestamp_t timestamp_venue_ns;   // Venue timestamp
    OrderStatus new_status;                     // Status update
    uint64_t fill_quantity;                     // If filled, how much
    Price32nd fill_price;                       // If filled, at what price
    char venue_order_id[16];                    // Venue tracking ID
    char reject_reason[24];                     // If rejected, why
    uint8_t _pad[7];                           // Pad to 64 bytes
};
static_assert(sizeof(VenueResponse) == CACHE_LINE_SIZE, "VenueResponse must be 64 bytes");

// ========================= 2. Venue Latency and Fill Models =========================

// Venue latency characteristics
struct VenueLatencyModel {
    HFTTimer::ns_t base_latency_ns;        // Base round-trip time (e.g., 50μs)
    HFTTimer::ns_t jitter_std_dev_ns;      // Gaussian jitter (e.g., 10μs)
    HFTTimer::ns_t queue_delay_ns;         // Additional delay under load
    double queue_probability;               // Probability of queue delay (0.0-1.0)
    
    // Calculate realistic venue latency
    [[nodiscard]] HFTTimer::ns_t calculate_latency() const noexcept {
        thread_local std::mt19937_64 rng(HFTTimer::get_cycles());
        thread_local std::normal_distribution<double> jitter_dist(0.0, 1.0);
        
        // Base latency + jitter
        HFTTimer::ns_t latency = base_latency_ns + 
            static_cast<HFTTimer::ns_t>(jitter_dist(rng) * jitter_std_dev_ns);
        
        // Queue delay with probability
        thread_local std::uniform_real_distribution<double> queue_dist(0.0, 1.0);
        if (queue_dist(rng) < queue_probability) {
            latency += queue_delay_ns;
        }
        
        return latency;
    }
};

// Fill probability and pricing model
class TreasuryFillModel {
public:
    // Calculate fill probability based on order aggressiveness
    [[nodiscard]] static double calculate_fill_probability(
        const TreasuryOrder& order,
        const TreasuryTick& current_market) noexcept {
        
        if (order.order_type == OrderType::Market) {
            return 0.95; // High probability for market orders
        }
        
        // For limit orders, compare to current market
        double limit_price = order.limit_price.to_decimal();
        if (order.side == OrderSide::Buy) {
            double ask_price = current_market.ask_price.to_decimal();
            if (limit_price >= ask_price) return 0.9;  // Aggressive buy
            // Linear decay based on price distance
            return std::max(0.0, 0.9 - (ask_price - limit_price) * 0.1);
        } else {
            double bid_price = current_market.bid_price.to_decimal();
            if (limit_price <= bid_price) return 0.9;  // Aggressive sell
            // Linear decay based on price distance
            return std::max(0.0, 0.9 - (limit_price - bid_price) * 0.1);
        }
    }
    
    // Determine fill price (price improvement modeling)
    [[nodiscard]] static Price32nd calculate_fill_price(
        const TreasuryOrder& order,
        const TreasuryTick& current_market) noexcept {
        
        if (order.order_type == OrderType::Market) {
            return order.side == OrderSide::Buy ? 
                current_market.ask_price : current_market.bid_price;
        }
        
        // For limit orders, model price improvement
        thread_local std::mt19937_64 rng(HFTTimer::get_cycles());
        thread_local std::uniform_real_distribution<double> price_dist(0.0, 1.0);
        
        if (order.side == OrderSide::Buy) {
            double ask = current_market.ask_price.to_decimal();
            double limit = order.limit_price.to_decimal();
            if (limit >= ask) {
                // Price improvement on aggressive orders
                double improved = ask - price_dist(rng) * 0.03125; // Up to 1/32 better
                return Price32nd::from_decimal(improved);
            }
            return order.limit_price;
        } else {
            double bid = current_market.bid_price.to_decimal();
            double limit = order.limit_price.to_decimal();
            if (limit <= bid) {
                // Price improvement on aggressive orders
                double improved = bid + price_dist(rng) * 0.03125; // Up to 1/32 better
                return Price32nd::from_decimal(improved);
            }
            return order.limit_price;
        }
    }
    
    // Model partial fill behavior
    [[nodiscard]] static uint64_t calculate_fill_quantity(
        const TreasuryOrder& order,
        double fill_probability) noexcept {
        
        thread_local std::mt19937_64 rng(HFTTimer::get_cycles());
        thread_local std::uniform_real_distribution<double> qty_dist(0.0, 1.0);
        
        // Higher fill probability = higher chance of full fill
        if (qty_dist(rng) < fill_probability) {
            return order.remaining_quantity;
        }
        
        // Partial fill based on probability
        uint64_t partial = static_cast<uint64_t>(
            order.remaining_quantity * fill_probability * qty_dist(rng));
        return std::max(partial, static_cast<uint64_t>(1)); // At least 1M
    }
};

// ========================= 3. Primary Dealer Venue Simulator =========================

class PrimaryDealerVenue {
public:
    // Constructor with venue configuration
    explicit PrimaryDealerVenue(
        std::string_view venue_name = "PrimaryDealer1",
        VenueLatencyModel latency_model = default_latency_model()) noexcept
        : venue_name_(venue_name)
        , latency_model_(latency_model)
        , current_market_{}
        , stats_{}
        , next_venue_order_id_(1)
        , active_orders_{}
        , active_order_count_(0)
        , response_buffer_() {
        
        // Initialize active orders array
        std::memset(active_orders_, 0, sizeof(active_orders_));
    }
    
    // Submit new order to venue
    [[nodiscard]] bool submit_order(const TreasuryOrder& order) noexcept {
        auto start = HFTTimer::get_cycles();
        
        // Validate order
        if (!validate_order(order)) {
            generate_reject_response(order, "Invalid order parameters");
            return false;
        }
        
        // Store in active orders (with bounds check)
        if (active_order_count_ >= MAX_ACTIVE_ORDERS) {
            generate_reject_response(order, "Venue capacity exceeded");
            return false;
        }
        
        // Copy order and update status
        TreasuryOrder& active_order = active_orders_[active_order_count_++];
        std::memcpy(&active_order, &order, sizeof(TreasuryOrder));
        active_order.status = OrderStatus::Submitted;
        active_order.timestamp_venue_ns = HFTTimer::get_timestamp_ns();
        
        // Generate venue order ID
        snprintf(active_order.venue_order_id, sizeof(active_order.venue_order_id),
                "V%lu", next_venue_order_id_++);
        
        // Schedule acknowledgment
        schedule_response(active_order, OrderStatus::Acknowledged);
        
        stats_.orders_submitted++;
        stats_.submit_latency_ns += HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
        
        return true;
    }
    
    // Cancel existing order
    [[nodiscard]] bool cancel_order(uint64_t order_id) noexcept {
        auto start = HFTTimer::get_cycles();
        
        // Find order
        TreasuryOrder* order = find_active_order(order_id);
        if (!order) {
            return false;
        }
        
        // Only allow cancellation of open orders
        if (order->status != OrderStatus::Acknowledged &&
            order->status != OrderStatus::PartiallyFilled) {
            return false;
        }
        
        // Schedule cancellation
        schedule_response(*order, OrderStatus::Cancelled);
        
        stats_.orders_cancelled++;
        stats_.cancel_latency_ns += HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
        
        return true;
    }
    
    // Process market data updates (affects fill decisions)
    void process_market_update(const TreasuryTick& tick) noexcept {
        auto start = HFTTimer::get_cycles();
        
        // Update current market view
        std::memcpy(&current_market_, &tick, sizeof(TreasuryTick));
        
        // Process fills for active orders
        for (size_t i = 0; i < active_order_count_; ++i) {
            TreasuryOrder& order = active_orders_[i];
            if (order.status != OrderStatus::Acknowledged &&
                order.status != OrderStatus::PartiallyFilled) {
                continue;
            }
            
            // Calculate fill probability
            double fill_prob = TreasuryFillModel::calculate_fill_probability(order, tick);
            
            // Determine if order gets filled
            thread_local std::mt19937_64 rng(HFTTimer::get_cycles());
            thread_local std::uniform_real_distribution<double> fill_dist(0.0, 1.0);
            
            if (fill_dist(rng) < fill_prob) {
                // Calculate fill quantity and price
                uint64_t fill_qty = TreasuryFillModel::calculate_fill_quantity(order, fill_prob);
                Price32nd fill_price = TreasuryFillModel::calculate_fill_price(order, tick);
                
                // Generate fill response
                VenueResponse response{};
                response.order_id = order.order_id;
                response.timestamp_venue_ns = HFTTimer::get_timestamp_ns() +
                    latency_model_.calculate_latency();
                response.fill_quantity = fill_qty;
                response.fill_price = fill_price;
                std::memcpy(response.venue_order_id, order.venue_order_id,
                          sizeof(response.venue_order_id));
                
                // Update order status
                order.filled_quantity += fill_qty;
                order.remaining_quantity -= fill_qty;
                
                if (order.remaining_quantity == 0) {
                    response.new_status = OrderStatus::Filled;
                    order.status = OrderStatus::Filled;
                    stats_.orders_filled++;
                } else {
                    response.new_status = OrderStatus::PartiallyFilled;
                    order.status = OrderStatus::PartiallyFilled;
                    stats_.orders_partially_filled++;
                }
                
                // Queue response
                response_buffer_.try_push(response);
            }
        }
        
        // Cleanup filled/cancelled orders
        cleanup_completed_orders();
        
        stats_.market_updates++;
        stats_.market_update_latency_ns += HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
    }
    
    // Get venue responses (fills, acks, rejects)
    [[nodiscard]] size_t get_venue_responses(
        VenueResponse* output_buffer,
        size_t max_count) noexcept {
        return response_buffer_.try_pop_batch(output_buffer, output_buffer + max_count);
    }
    
    // Update venue state (process pending orders, generate fills)
    void update_venue_state() noexcept {
        auto start = HFTTimer::get_cycles();
        
        // Process scheduled responses
        process_pending_responses();
        
        // Update statistics
        stats_.venue_updates++;
        stats_.venue_update_latency_ns += HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
    }
    
    // Venue statistics and monitoring
    struct VenueStats {
        uint64_t orders_submitted = 0;
        uint64_t orders_acknowledged = 0;
        uint64_t orders_filled = 0;
        uint64_t orders_partially_filled = 0;
        uint64_t orders_rejected = 0;
        uint64_t orders_cancelled = 0;
        uint64_t market_updates = 0;
        uint64_t venue_updates = 0;
        HFTTimer::ns_t submit_latency_ns = 0;
        HFTTimer::ns_t cancel_latency_ns = 0;
        HFTTimer::ns_t market_update_latency_ns = 0;
        HFTTimer::ns_t venue_update_latency_ns = 0;
        double fill_rate = 0.0;
    };
    
    [[nodiscard]] VenueStats get_venue_stats() const noexcept {
        VenueStats stats = stats_;
        if (stats.orders_submitted > 0) {
            stats.fill_rate = static_cast<double>(stats.orders_filled) /
                            static_cast<double>(stats.orders_submitted);
        }
        return stats;
    }
    
    void reset_stats() noexcept {
        stats_ = {};
    }

private:
    static constexpr size_t MAX_ACTIVE_ORDERS = 4096;
    static constexpr size_t RESPONSE_BUFFER_SIZE = 8192;
    
    // Venue configuration
    std::string_view venue_name_;
    VenueLatencyModel latency_model_;
    TreasuryTick current_market_;
    VenueStats stats_;
    uint64_t next_venue_order_id_;
    
    // Order management
    std::array<TreasuryOrder, MAX_ACTIVE_ORDERS> active_orders_;
    size_t active_order_count_;
    
    // Response management
    using ResponseBuffer = SPSCRingBuffer<VenueResponse, RESPONSE_BUFFER_SIZE>;
    ResponseBuffer response_buffer_;
    
    // Default latency model
    [[nodiscard]] static VenueLatencyModel default_latency_model() noexcept {
        return VenueLatencyModel{
            .base_latency_ns = 50'000,      // 50μs base latency
            .jitter_std_dev_ns = 10'000,    // 10μs jitter
            .queue_delay_ns = 100'000,      // 100μs queue delay
            .queue_probability = 0.1         // 10% chance of queuing
        };
    }
    
    // Validate incoming order
    [[nodiscard]] bool validate_order(const TreasuryOrder& order) const noexcept {
        if (order.order_type == OrderType::Invalid) return false;
        if (order.quantity == 0) return false;
        if (order.order_type == OrderType::Limit && order.limit_price.whole == 0) return false;
        if (order.order_type == OrderType::YieldLimit && order.yield_limit <= 0.0) return false;
        return true;
    }
    
    // Find active order by ID
    [[nodiscard]] TreasuryOrder* find_active_order(uint64_t order_id) noexcept {
        for (size_t i = 0; i < active_order_count_; ++i) {
            if (active_orders_[i].order_id == order_id) {
                return &active_orders_[i];
            }
        }
        return nullptr;
    }
    
    // Schedule a response for an order
    void schedule_response(const TreasuryOrder& order, OrderStatus new_status) noexcept {
        VenueResponse response{};
        response.order_id = order.order_id;
        response.timestamp_venue_ns = HFTTimer::get_timestamp_ns() +
            latency_model_.calculate_latency();
        response.new_status = new_status;
        std::memcpy(response.venue_order_id, order.venue_order_id,
                   sizeof(response.venue_order_id));
        
        response_buffer_.try_push(response);
        
        if (new_status == OrderStatus::Acknowledged) {
            stats_.orders_acknowledged++;
        }
    }
    
    // Generate rejection response
    void generate_reject_response(
        const TreasuryOrder& order,
        const char* reason) noexcept {
        VenueResponse response{};
        response.order_id = order.order_id;
        response.timestamp_venue_ns = HFTTimer::get_timestamp_ns();
        response.new_status = OrderStatus::Rejected;
        std::strncpy(response.reject_reason, reason,
                    sizeof(response.reject_reason) - 1);
        
        response_buffer_.try_push(response);
        stats_.orders_rejected++;
    }
    
    // Process all pending responses
    void process_pending_responses() noexcept {
        // No-op for now, responses are sent immediately
        // Could be extended to implement more complex response scheduling
    }
    
    // Cleanup completed orders
    void cleanup_completed_orders() noexcept {
        size_t write = 0;
        for (size_t read = 0; read < active_order_count_; ++read) {
            if (active_orders_[read].status != OrderStatus::Filled &&
                active_orders_[read].status != OrderStatus::Cancelled &&
                active_orders_[read].status != OrderStatus::Rejected) {
                if (write != read) {
                    std::memcpy(&active_orders_[write], &active_orders_[read],
                              sizeof(TreasuryOrder));
                }
                ++write;
            }
        }
        active_order_count_ = write;
    }
};

// ========================= 4. Order Management Integration =========================

class TreasuryOrderRouter {
public:
    TreasuryOrderRouter() noexcept {}
    
    // Register venue for order routing
    void add_venue(std::unique_ptr<PrimaryDealerVenue> venue) noexcept {
        if (venue_count_ < MAX_VENUES) {
            venues_[venue_count_++] = std::move(venue);
        }
    }
    
    // Route order to appropriate venue
    [[nodiscard]] bool route_order(const TreasuryOrder& order) noexcept {
        auto start = HFTTimer::get_cycles();
        
        // Simple round-robin venue selection
        if (venue_count_ == 0) return false;
        
        size_t venue_idx = next_venue_++ % venue_count_;
        bool result = venues_[venue_idx]->submit_order(order);
        
        routing_latency_hist_.record_latency(
            HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start));
        
        return result;
    }
    
    // Process all venue responses
    [[nodiscard]] size_t process_venue_responses() noexcept {
        size_t total_processed = 0;
        VenueResponse responses[64];  // Process in small batches
        
        for (size_t i = 0; i < venue_count_; ++i) {
            size_t count = venues_[i]->get_venue_responses(responses, 64);
            if (count > 0) {
                consolidated_responses_.try_push_batch(responses, responses + count);
                total_processed += count;
            }
        }
        
        return total_processed;
    }
    
    // Get filled orders for strategy processing
    [[nodiscard]] size_t get_fills(
        VenueResponse* output_buffer,
        size_t max_count) noexcept {
        return consolidated_responses_.try_pop_batch(
            output_buffer, output_buffer + max_count);
    }
    
    // Market data integration (feed to all venues)
    void process_market_data(const TreasuryTick& tick) noexcept {
        for (size_t i = 0; i < venue_count_; ++i) {
            venues_[i]->process_market_data(tick);
        }
    }
    
    // Performance monitoring
    [[nodiscard]] HFTTimer::ns_t get_avg_routing_latency() const noexcept {
        auto stats = routing_latency_hist_.get_stats();
        return static_cast<HFTTimer::ns_t>(stats.mean_latency);
    }

private:
    static constexpr size_t MAX_VENUES = 8;
    static constexpr size_t RESPONSE_BUFFER_SIZE = 16384;
    
    std::array<std::unique_ptr<PrimaryDealerVenue>, MAX_VENUES> venues_;
    size_t venue_count_ = 0;
    std::atomic<size_t> next_venue_{0};
    
    SPSCRingBuffer<VenueResponse, RESPONSE_BUFFER_SIZE> consolidated_responses_;
    LatencyHistogram routing_latency_hist_;
};

// ========================= 5. Strategy Integration Framework =========================

class TestMarketMakingStrategy {
public:
    explicit TestMarketMakingStrategy(TreasuryOrderRouter& router) noexcept
        : router_(router)
        , next_order_id_(1)
        , stats_{} {}
    
    // Process incoming market data and generate orders
    void process_market_tick(const TreasuryTick& tick) noexcept {
        auto [bid_price, ask_price] = calculate_quotes(tick);
        
        // Generate buy order
        TreasuryOrder buy_order{};
        buy_order.order_id = next_order_id_++;
        buy_order.timestamp_created_ns = HFTTimer::get_timestamp_ns();
        buy_order.instrument_type = tick.instrument_type;
        buy_order.order_type = OrderType::Limit;
        buy_order.side = OrderSide::Buy;
        buy_order.limit_price = bid_price;
        buy_order.quantity = 10;  // 10M face value
        
        if (router_.route_order(buy_order)) {
            stats_.orders_sent++;
        }
        
        // Generate sell order
        TreasuryOrder sell_order{};
        sell_order.order_id = next_order_id_++;
        sell_order.timestamp_created_ns = HFTTimer::get_timestamp_ns();
        sell_order.instrument_type = tick.instrument_type;
        sell_order.order_type = OrderType::Limit;
        sell_order.side = OrderSide::Sell;
        sell_order.limit_price = ask_price;
        sell_order.quantity = 10;  // 10M face value
        
        if (router_.route_order(sell_order)) {
            stats_.orders_sent++;
        }
    }
    
    // Handle order fills and update positions
    void process_fill(const VenueResponse& fill) noexcept {
        stats_.fills_received++;
        
        // Update position tracking
        TreasuryOrder* order = find_order(fill.order_id);
        if (order) {
            int64_t fill_delta = static_cast<int64_t>(fill.fill_quantity);
            if (order->side == OrderSide::Sell) {
                fill_delta = -fill_delta;
            }
            stats_.net_position += fill_delta;
            
            // Simple P&L calculation
            double fill_price = fill.fill_price.to_decimal();
            stats_.pnl_unrealized += fill_delta * (100.0 - fill_price);
        }
    }
    
    // Get strategy performance metrics
    struct StrategyStats {
        uint64_t orders_sent = 0;
        uint64_t fills_received = 0;
        double pnl_unrealized = 0.0;
        int64_t net_position = 0;
    };
    
    [[nodiscard]] StrategyStats get_strategy_stats() const noexcept {
        return stats_;
    }

private:
    TreasuryOrderRouter& router_;
    std::atomic<uint64_t> next_order_id_;
    StrategyStats stats_;
    
    // Simple spread calculation
    [[nodiscard]] std::pair<Price32nd, Price32nd> calculate_quotes(
        const TreasuryTick& tick) const noexcept {
        // Add 1/32nd to bid, subtract 1/32nd from ask
        double bid = tick.bid_price.to_decimal() + 0.03125;
        double ask = tick.ask_price.to_decimal() - 0.03125;
        return {Price32nd::from_decimal(bid), Price32nd::from_decimal(ask)};
    }
    
    // Placeholder for order tracking (would need proper order management)
    [[nodiscard]] TreasuryOrder* find_order(uint64_t /*order_id*/) noexcept {
        return nullptr;  // TODO: Implement order tracking
    }
};

} // namespace market_data
} // namespace hft 