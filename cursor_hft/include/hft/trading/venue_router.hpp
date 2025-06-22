#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include <cmath>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/trading/order_lifecycle_manager.hpp"

namespace hft {
namespace trading {

using namespace hft::market_data;

/**
 * @brief Advanced multi-venue order routing with real-time optimization
 * 
 * Implements intelligent order routing across multiple venues:
 * - Real-time venue performance monitoring
 * - Dynamic fill probability calculation
 * - Latency-aware routing decisions
 * - Smart order routing (SOR) algorithms
 * - Venue connectivity monitoring
 * - Load balancing and failover
 * 
 * Performance targets:
 * - Routing decision: <200ns
 * - Venue selection: <100ns
 * - Performance update: <50ns
 * - Connectivity check: <25ns
 * - SOR algorithm: <300ns
 */
class alignas(64) VenueRouter {
public:
    static constexpr size_t MAX_VENUES = 8;
    static constexpr size_t PERFORMANCE_HISTORY_SIZE = 1000;
    static constexpr size_t CONNECTIVITY_HISTORY_SIZE = 100;
    
    // Venue connection status
    enum class ConnectionStatus : uint8_t {
        CONNECTED = 0,
        DISCONNECTED = 1,
        RECONNECTING = 2,
        DEGRADED = 3,
        MAINTENANCE = 4,
        SUSPENDED = 5
    };
    
    // Smart order routing strategy
    enum class SORStrategy : uint8_t {
        LATENCY_OPTIMIZED = 0,    // Minimize latency
        FILL_RATE_OPTIMIZED = 1,  // Maximize fill probability
        COST_OPTIMIZED = 2,       // Minimize trading costs
        BALANCED = 3,             // Balance all factors
        VOLUME_WEIGHTED = 4,      // Route based on venue volume
        ICEBERG = 5              // Hide large orders
    };
    
    // Real-time venue performance metrics
    struct alignas(64) VenuePerformance {
        VenueType venue = VenueType::PRIMARY_DEALER;        // Venue identifier (1 byte)
        ConnectionStatus status = ConnectionStatus::CONNECTED; // Connection status (1 byte)
        uint8_t _pad0[6];                                   // Padding (6 bytes)
        double fill_rate = 1.0;                             // Current fill rate (8 bytes)
        double average_latency_ns = 1000.0;                 // Average response time (8 bytes)
        double market_impact = 0.001;                       // Price impact factor (8 bytes)
        double available_liquidity = 1000000000.0;          // Available liquidity estimate (8 bytes)
        uint64_t total_volume_traded = 0;                   // Total volume today (8 bytes)
        uint64_t last_response_time_ns = 0;                 // Last response timestamp (8 bytes)
        uint64_t connection_uptime_ns = 0;                  // Connection uptime (8 bytes)
        // Total: 1+1+6+8+8+8+8+8+8+8 = 64 bytes
        
        VenuePerformance() noexcept : _pad0{} {}
    };
    static_assert(sizeof(VenuePerformance) == 64, "VenuePerformance must be 64 bytes");
    
    // Routing decision output
    struct RoutingDecision {
        VenueType primary_venue = VenueType::PRIMARY_DEALER;
        VenueType backup_venue = VenueType::ECN;
        double confidence_score = 1.0;                      // Decision confidence [0,1]
        double expected_fill_rate = 1.0;                    // Expected fill probability
        double expected_latency_ns = 1000.0;                // Expected response time
        uint64_t decision_time_ns = 0;                      // Decision timestamp
        SORStrategy strategy_used = SORStrategy::BALANCED;   // Strategy applied
    };
    
    // Venue-specific order characteristics
    struct OrderCharacteristics {
        TreasuryType instrument;
        OrderSide side;
        uint64_t quantity;
        Price32nd price;
        bool is_aggressive;                                 // Market vs limit order
        double urgency_factor;                              // Urgency [0,1]
        uint64_t max_acceptable_latency_ns;                 // Latency threshold
    };
    
    // Historical performance tracking
    struct alignas(64) PerformanceHistory {
        std::array<uint64_t, PERFORMANCE_HISTORY_SIZE> latencies;
        std::array<bool, PERFORMANCE_HISTORY_SIZE> fill_results;
        std::array<double, PERFORMANCE_HISTORY_SIZE> market_impacts;
        size_t current_index = 0;
        uint64_t sample_count = 0;
        
        PerformanceHistory() noexcept : latencies{}, fill_results{}, market_impacts{} {}
    };
    
    // Connectivity monitoring
    struct alignas(64) ConnectivityMonitor {
        std::array<ConnectionStatus, CONNECTIVITY_HISTORY_SIZE> status_history;
        std::array<uint64_t, CONNECTIVITY_HISTORY_SIZE> status_timestamps;
        size_t current_index = 0;
        uint64_t last_heartbeat_ns = 0;
        uint64_t disconnect_count = 0;
        double uptime_percentage = 100.0;
        
        ConnectivityMonitor() noexcept : status_history{}, status_timestamps{} {}
    };
    
    /**
     * @brief Constructor with performance optimization
     */
    VenueRouter() noexcept;
    
    // No copy or move semantics
    VenueRouter(const VenueRouter&) = delete;
    VenueRouter& operator=(const VenueRouter&) = delete;
    
    /**
     * @brief Select optimal venue for order routing
     * @param characteristics Order characteristics
     * @param strategy Routing strategy to apply
     * @return Routing decision with venue selection
     */
    [[nodiscard]] RoutingDecision route_order(
        const OrderCharacteristics& characteristics,
        SORStrategy strategy = SORStrategy::BALANCED
    ) noexcept;
    
    /**
     * @brief Update venue performance metrics
     * @param venue Venue to update
     * @param latency_ns Response latency
     * @param filled Whether order was filled
     * @param market_impact Price impact observed
     */
    void update_venue_performance(
        VenueType venue,
        uint64_t latency_ns,
        bool filled,
        double market_impact
    ) noexcept;
    
    /**
     * @brief Update venue connectivity status
     * @param venue Venue to update
     * @param status New connection status
     */
    void update_connectivity_status(VenueType venue, ConnectionStatus status) noexcept;
    
    /**
     * @brief Get current venue performance
     * @param venue Venue to query
     * @return Current performance metrics
     */
    [[nodiscard]] const VenuePerformance& get_venue_performance(VenueType venue) const noexcept;
    
    /**
     * @brief Get venue ranking for specific instrument
     * @param instrument Treasury instrument
     * @param side Order side
     * @return Venues ranked by suitability
     */
    [[nodiscard]] std::array<VenueType, MAX_VENUES> get_venue_ranking(
        TreasuryType instrument,
        OrderSide side
    ) const noexcept;
    
    /**
     * @brief Check if venue is available for trading
     * @param venue Venue to check
     * @return true if venue is available
     */
    [[nodiscard]] bool is_venue_available(VenueType venue) const noexcept;
    
    /**
     * @brief Get smart order routing recommendation
     * @param characteristics Order characteristics
     * @return SOR strategy recommendation
     */
    [[nodiscard]] SORStrategy recommend_sor_strategy(const OrderCharacteristics& characteristics) const noexcept;
    
    /**
     * @brief Calculate expected fill probability
     * @param venue Target venue
     * @param characteristics Order characteristics
     * @return Fill probability [0,1]
     */
    [[nodiscard]] double calculate_fill_probability(
        VenueType venue,
        const OrderCharacteristics& characteristics
    ) const noexcept;
    
    /**
     * @brief Enable/disable venue for routing
     * @param venue Venue to configure
     * @param enabled Enable/disable flag
     */
    void set_venue_enabled(VenueType venue, bool enabled) noexcept;

private:
    // Performance tracking per venue
    alignas(64) std::array<VenuePerformance, MAX_VENUES> venue_performance_;
    alignas(64) std::array<PerformanceHistory, MAX_VENUES> performance_history_;
    alignas(64) std::array<ConnectivityMonitor, MAX_VENUES> connectivity_monitors_;
    
    // Routing configuration
    alignas(64) std::array<bool, MAX_VENUES> venue_enabled_;
    alignas(64) hft::HFTTimer timer_;
    
    // Performance optimization caches
    alignas(64) std::array<uint64_t, MAX_VENUES> last_performance_update_;
    alignas(64) std::array<double, MAX_VENUES> cached_venue_scores_;
    alignas(64) std::atomic<uint64_t> cache_invalidation_time_;
    
    // Algorithm implementations
    RoutingDecision apply_latency_optimization(const OrderCharacteristics& characteristics) noexcept;
    RoutingDecision apply_fill_rate_optimization(const OrderCharacteristics& characteristics) noexcept;
    RoutingDecision apply_cost_optimization(const OrderCharacteristics& characteristics) noexcept;
    RoutingDecision apply_balanced_optimization(const OrderCharacteristics& characteristics) noexcept;
    RoutingDecision apply_volume_weighted_routing(const OrderCharacteristics& characteristics) noexcept;
    
    // Utility methods
    double calculate_venue_score(VenueType venue, const OrderCharacteristics& characteristics, SORStrategy strategy) noexcept;
    void update_performance_cache(VenueType venue) noexcept;
    bool is_cache_valid() const noexcept;
    void invalidate_cache() noexcept;
    
    double calculate_latency_percentile(VenueType venue, double percentile) const noexcept;
    double calculate_historical_fill_rate(VenueType venue, uint64_t lookback_samples) const noexcept;
    double calculate_market_impact_estimate(VenueType venue, uint64_t quantity) const noexcept;
};

// Implementation

inline VenueRouter::VenueRouter() noexcept
    : venue_performance_{},
      performance_history_{},
      connectivity_monitors_{},
      venue_enabled_{},
      timer_(),
      last_performance_update_{},
      cached_venue_scores_{},
      cache_invalidation_time_(0) {
    
    // Initialize venue performance with defaults
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        auto& perf = venue_performance_[i];
        perf.venue = static_cast<VenueType>(i);
        perf.status = ConnectionStatus::CONNECTED;
        perf.fill_rate = 0.95 - i * 0.05; // Decreasing fill rates
        perf.average_latency_ns = 1000.0 + i * 200.0; // Increasing latencies
        perf.market_impact = 0.001 + i * 0.0005; // Increasing impact
        perf.available_liquidity = 1000000000.0 - i * 100000000.0; // Decreasing liquidity
        
        venue_enabled_[i] = true;
        last_performance_update_[i] = 0;
        cached_venue_scores_[i] = 0.0;
    }
}

inline VenueRouter::RoutingDecision VenueRouter::route_order(
    const OrderCharacteristics& characteristics,
    SORStrategy strategy
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    RoutingDecision decision;
    decision.decision_time_ns = start_time;
    decision.strategy_used = strategy;
    
    // Apply selected strategy
    switch (strategy) {
        case SORStrategy::LATENCY_OPTIMIZED:
            decision = apply_latency_optimization(characteristics);
            break;
        case SORStrategy::FILL_RATE_OPTIMIZED:
            decision = apply_fill_rate_optimization(characteristics);
            break;
        case SORStrategy::COST_OPTIMIZED:
            decision = apply_cost_optimization(characteristics);
            break;
        case SORStrategy::BALANCED:
            decision = apply_balanced_optimization(characteristics);
            break;
        case SORStrategy::VOLUME_WEIGHTED:
            decision = apply_volume_weighted_routing(characteristics);
            break;
        default:
            decision = apply_balanced_optimization(characteristics);
            break;
    }
    
    decision.decision_time_ns = start_time;
    decision.strategy_used = strategy;
    
    return decision;
}

inline VenueRouter::RoutingDecision VenueRouter::apply_latency_optimization(
    const OrderCharacteristics& characteristics
) noexcept {
    RoutingDecision decision;
    
    VenueType best_venue = VenueType::PRIMARY_DEALER;
    double best_latency = 1e9; // Large initial value
    
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        if (!venue_enabled_[i]) continue;
        
        const auto venue = static_cast<VenueType>(i);
        const auto& perf = venue_performance_[i];
        
        if (perf.status != ConnectionStatus::CONNECTED) continue;
        
        // Consider 95th percentile latency for reliability
        const double p95_latency = calculate_latency_percentile(venue, 0.95);
        
        if (p95_latency < best_latency) {
            best_latency = p95_latency;
            best_venue = venue;
        }
    }
    
    decision.primary_venue = best_venue;
    decision.expected_latency_ns = best_latency;
    decision.expected_fill_rate = venue_performance_[static_cast<size_t>(best_venue)].fill_rate;
    decision.confidence_score = 0.9; // High confidence for latency optimization
    
    // Select backup venue (second best latency)
    VenueType backup_venue = VenueType::ECN;
    double second_best_latency = 1e9;
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        if (!venue_enabled_[i]) continue;
        const auto venue = static_cast<VenueType>(i);
        if (venue == best_venue) continue;
        
        const double latency = calculate_latency_percentile(venue, 0.95);
        if (latency < second_best_latency) {
            second_best_latency = latency;
            backup_venue = venue;
        }
    }
    decision.backup_venue = backup_venue;
    
    return decision;
}

inline VenueRouter::RoutingDecision VenueRouter::apply_fill_rate_optimization(
    const OrderCharacteristics& characteristics
) noexcept {
    RoutingDecision decision;
    
    VenueType best_venue = VenueType::PRIMARY_DEALER;
    double best_fill_rate = 0.0;
    
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        if (!venue_enabled_[i]) continue;
        
        const auto venue = static_cast<VenueType>(i);
        const auto& perf = venue_performance_[i];
        
        if (perf.status != ConnectionStatus::CONNECTED) continue;
        
        // Calculate expected fill probability for this order
        const double fill_probability = calculate_fill_probability(venue, characteristics);
        
        if (fill_probability > best_fill_rate) {
            best_fill_rate = fill_probability;
            best_venue = venue;
        }
    }
    
    decision.primary_venue = best_venue;
    decision.expected_fill_rate = best_fill_rate;
    decision.expected_latency_ns = venue_performance_[static_cast<size_t>(best_venue)].average_latency_ns;
    decision.confidence_score = best_fill_rate;
    
    return decision;
}

inline VenueRouter::RoutingDecision VenueRouter::apply_balanced_optimization(
    const OrderCharacteristics& characteristics
) noexcept {
    RoutingDecision decision;
    
    VenueType best_venue = VenueType::PRIMARY_DEALER;
    double best_score = -1.0;
    
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        if (!venue_enabled_[i]) continue;
        
        const auto venue = static_cast<VenueType>(i);
        const double score = calculate_venue_score(venue, characteristics, SORStrategy::BALANCED);
        
        if (score > best_score) {
            best_score = score;
            best_venue = venue;
        }
    }
    
    decision.primary_venue = best_venue;
    decision.confidence_score = std::min(1.0, best_score);
    
    const auto& perf = venue_performance_[static_cast<size_t>(best_venue)];
    decision.expected_fill_rate = perf.fill_rate;
    decision.expected_latency_ns = perf.average_latency_ns;
    
    return decision;
}

inline double VenueRouter::calculate_venue_score(
    VenueType venue,
    const OrderCharacteristics& characteristics,
    SORStrategy strategy
) noexcept {
    const auto venue_index = static_cast<size_t>(venue);
    if (venue_index >= MAX_VENUES || !venue_enabled_[venue_index]) {
        return 0.0;
    }
    
    const auto& perf = venue_performance_[venue_index];
    if (perf.status != ConnectionStatus::CONNECTED) {
        return 0.0;
    }
    
    // Weighted scoring based on strategy
    double score = 0.0;
    
    switch (strategy) {
        case SORStrategy::BALANCED:
            // Equal weight to fill rate, latency, and market impact
            score = (perf.fill_rate * 0.4) +
                   (1.0 / (1.0 + perf.average_latency_ns / 1000.0) * 0.3) +
                   (1.0 / (1.0 + perf.market_impact * 1000.0) * 0.3);
            break;
            
        case SORStrategy::LATENCY_OPTIMIZED:
            // Heavy weight on latency
            score = (1.0 / (1.0 + perf.average_latency_ns / 1000.0) * 0.7) +
                   (perf.fill_rate * 0.3);
            break;
            
        case SORStrategy::FILL_RATE_OPTIMIZED:
            // Heavy weight on fill rate
            score = (perf.fill_rate * 0.8) +
                   (1.0 / (1.0 + perf.average_latency_ns / 1000.0) * 0.2);
            break;
            
        default:
            score = perf.fill_rate;
            break;
    }
    
    // Adjust for order characteristics
    if (characteristics.is_aggressive) {
        score *= 1.1; // Boost score for market orders
    }
    
    // Adjust for urgency
    if (characteristics.urgency_factor > 0.8) {
        score *= (1.0 + characteristics.urgency_factor * 0.2);
    }
    
    return std::max(0.0, std::min(1.0, score));
}

inline void VenueRouter::update_venue_performance(
    VenueType venue,
    uint64_t latency_ns,
    bool filled,
    double market_impact
) noexcept {
    const auto venue_index = static_cast<size_t>(venue);
    if (venue_index >= MAX_VENUES) return;
    
    auto& perf = venue_performance_[venue_index];
    auto& history = performance_history_[venue_index];
    
    // Update performance history
    const size_t index = history.current_index % PERFORMANCE_HISTORY_SIZE;
    history.latencies[index] = latency_ns;
    history.fill_results[index] = filled;
    history.market_impacts[index] = market_impact;
    
    history.current_index++;
    history.sample_count = std::min(history.sample_count + 1, static_cast<uint64_t>(PERFORMANCE_HISTORY_SIZE));
    
    // Update rolling averages (exponential moving average)
    constexpr double alpha = 0.1;
    
    perf.average_latency_ns = perf.average_latency_ns * (1.0 - alpha) + static_cast<double>(latency_ns) * alpha;
    perf.market_impact = perf.market_impact * (1.0 - alpha) + market_impact * alpha;
    
    // Update fill rate
    if (filled) {
        perf.fill_rate = perf.fill_rate * (1.0 - alpha) + alpha;
    } else {
        perf.fill_rate = perf.fill_rate * (1.0 - alpha);
    }
    
    perf.last_response_time_ns = timer_.get_timestamp_ns();
    
    // Invalidate cache
    invalidate_cache();
}

inline double VenueRouter::calculate_fill_probability(
    VenueType venue,
    const OrderCharacteristics& characteristics
) const noexcept {
    const auto venue_index = static_cast<size_t>(venue);
    if (venue_index >= MAX_VENUES) return 0.0;
    
    const auto& perf = venue_performance_[venue_index];
    
    // Base fill rate
    double probability = perf.fill_rate;
    
    // Adjust for order aggressiveness
    if (characteristics.is_aggressive) {
        probability *= 1.2; // Market orders more likely to fill
    } else {
        probability *= 0.9; // Limit orders less likely to fill
    }
    
    // Adjust for order size relative to available liquidity
    const double size_ratio = static_cast<double>(characteristics.quantity) / perf.available_liquidity;
    if (size_ratio > 0.1) {
        probability *= (1.0 - size_ratio * 0.5); // Large orders less likely to fill completely
    }
    
    // Adjust for venue-specific instrument preferences
    switch (characteristics.instrument) {
        case TreasuryType::Bill_3M:
        case TreasuryType::Bill_6M:
            // Primary dealers better for bills
            if (venue == VenueType::PRIMARY_DEALER) probability *= 1.1;
            break;
        case TreasuryType::Bond_30Y:
            // ECNs sometimes better for bonds
            if (venue == VenueType::ECN) probability *= 1.05;
            break;
        default:
            break;
    }
    
    return std::max(0.0, std::min(1.0, probability));
}

// Stub implementations for remaining methods
inline VenueRouter::RoutingDecision VenueRouter::apply_cost_optimization(const OrderCharacteristics& characteristics) noexcept {
    // Would implement cost-based optimization considering fees, spreads, etc.
    return apply_balanced_optimization(characteristics);
}

inline VenueRouter::RoutingDecision VenueRouter::apply_volume_weighted_routing(const OrderCharacteristics& characteristics) noexcept {
    // Would implement volume-weighted routing
    return apply_balanced_optimization(characteristics);
}

inline void VenueRouter::update_connectivity_status(VenueType venue, ConnectionStatus status) noexcept {
    const auto venue_index = static_cast<size_t>(venue);
    if (venue_index >= MAX_VENUES) return;
    
    auto& perf = venue_performance_[venue_index];
    auto& monitor = connectivity_monitors_[venue_index];
    
    perf.status = status;
    
    // Update connectivity history
    const size_t index = monitor.current_index % CONNECTIVITY_HISTORY_SIZE;
    monitor.status_history[index] = status;
    monitor.status_timestamps[index] = timer_.get_timestamp_ns();
    monitor.current_index++;
    
    if (status == ConnectionStatus::DISCONNECTED) {
        monitor.disconnect_count++;
    }
}

inline const VenueRouter::VenuePerformance& VenueRouter::get_venue_performance(VenueType venue) const noexcept {
    const auto index = static_cast<size_t>(venue);
    return (index < MAX_VENUES) ? venue_performance_[index] : venue_performance_[0];
}

inline bool VenueRouter::is_venue_available(VenueType venue) const noexcept {
    const auto index = static_cast<size_t>(venue);
    if (index >= MAX_VENUES) return false;
    
    return venue_enabled_[index] && venue_performance_[index].status == ConnectionStatus::CONNECTED;
}

inline double VenueRouter::calculate_latency_percentile(VenueType venue, double percentile) const noexcept {
    const auto venue_index = static_cast<size_t>(venue);
    if (venue_index >= MAX_VENUES) return 1e9;
    
    const auto& history = performance_history_[venue_index];
    if (history.sample_count == 0) {
        return venue_performance_[venue_index].average_latency_ns;
    }
    
    // Simple percentile calculation (would use more sophisticated method in production)
    const size_t target_index = static_cast<size_t>(percentile * std::min(history.sample_count, static_cast<uint64_t>(PERFORMANCE_HISTORY_SIZE)));
    return static_cast<double>(history.latencies[target_index]);
}

inline void VenueRouter::invalidate_cache() noexcept {
    cache_invalidation_time_.store(timer_.get_timestamp_ns(), std::memory_order_relaxed);
}

inline void VenueRouter::set_venue_enabled(VenueType venue, bool enabled) noexcept {
    const auto index = static_cast<size_t>(venue);
    if (index < MAX_VENUES) {
        venue_enabled_[index] = enabled;
        invalidate_cache();
    }
}

// Stub implementations for remaining getter methods
inline std::array<VenueType, VenueRouter::MAX_VENUES> VenueRouter::get_venue_ranking(
    TreasuryType instrument,
    OrderSide side
) const noexcept {
    std::array<VenueType, MAX_VENUES> ranking;
    for (size_t i = 0; i < MAX_VENUES; ++i) {
        ranking[i] = static_cast<VenueType>(i);
    }
    return ranking;
}

inline VenueRouter::SORStrategy VenueRouter::recommend_sor_strategy(const OrderCharacteristics& characteristics) const noexcept {
    // Simple recommendation logic
    if (characteristics.urgency_factor > 0.8) {
        return SORStrategy::LATENCY_OPTIMIZED;
    } else if (characteristics.quantity > 10000000) {
        return SORStrategy::VOLUME_WEIGHTED;
    } else {
        return SORStrategy::BALANCED;
    }
}

// Static asserts
static_assert(alignof(VenueRouter) == 64, "VenueRouter must be cache-aligned");

} // namespace trading
} // namespace hft