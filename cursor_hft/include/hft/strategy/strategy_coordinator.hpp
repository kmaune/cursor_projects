#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include <type_traits>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/strategy/simple_market_maker.hpp"

namespace hft {
namespace strategy {

using namespace hft::market_data;

/**
 * @brief High-performance strategy coordination framework
 * 
 * Manages multiple trading strategies with lock-free communication and
 * sub-100ns coordination overhead. Provides:
 * - Template-based strategy dispatching for zero-overhead abstraction
 * - Priority-based resource allocation
 * - Cross-strategy position netting
 * - Lock-free inter-strategy communication
 * - Real-time performance monitoring
 * 
 * Design principles:
 * - Cache-aligned data structures for ARM64
 * - Zero allocation in hot paths
 * - Template metaprogramming for compile-time optimization
 * - Ring buffer-based messaging for predictable latency
 * 
 * Performance targets:
 * - Strategy coordination: <100ns overhead
 * - Strategy dispatch: <50ns per strategy
 * - Position netting: <200ns across all strategies
 * - Communication latency: <30ns per message
 */
template<typename... StrategyTypes>
class alignas(64) StrategyCoordinator {
public:
    static constexpr size_t MAX_STRATEGIES = sizeof...(StrategyTypes);
    static constexpr size_t MAX_INSTRUMENTS = 6;  // Treasury instrument count
    static constexpr size_t MESSAGE_BUFFER_SIZE = 4096;
    
    // Strategy execution result
    struct alignas(64) StrategyResult {
        enum class Action : uint8_t {
            NO_ACTION = 0,
            UPDATE_QUOTES = 1,
            CANCEL_QUOTES = 2,
            POSITION_UPDATE = 3,
            RISK_LIMIT_HIT = 4
        };
        
        Action action = Action::NO_ACTION;              // 1 byte
        uint8_t strategy_id = 0;                        // 1 byte
        uint8_t priority = 0;                           // 1 byte (0 = highest)
        uint8_t _pad0[5];                               // 5 bytes padding
        TreasuryType instrument;                        // 1 byte
        uint8_t _pad1[3];                               // 3 bytes padding  
        uint32_t _reserved = 0;                         // 4 bytes reserved
        Price32nd bid_price;                            // 8 bytes
        Price32nd ask_price;                            // 8 bytes
        uint64_t bid_size = 0;                          // 8 bytes
        uint64_t ask_size = 0;                          // 8 bytes
        uint64_t execution_time_ns = 0;                 // 8 bytes
        uint64_t coordination_overhead_ns = 0;          // 8 bytes
        // Total: 8 + 8 + 8 + 8 + 8 + 8 + 8 + 8 = 64 bytes
        
        StrategyResult() noexcept : _pad0{}, _pad1{} {}
    };
    static_assert(sizeof(StrategyResult) == 64, "StrategyResult must be 64 bytes");
    
    // Strategy configuration and priority
    struct StrategyConfig {
        uint8_t priority = 0;                           // 0 = highest priority
        bool enabled = true;                            // Strategy enabled/disabled
        double resource_allocation = 1.0;               // Resource allocation factor
        uint64_t max_position_size = 50000000;          // Per-strategy position limit
        uint32_t risk_score_limit = 800;                // Risk score threshold
        
        StrategyConfig() noexcept = default;
        StrategyConfig(uint8_t prio, bool en = true, double alloc = 1.0) noexcept
            : priority(prio), enabled(en), resource_allocation(alloc) {}
    };
    
    // Cross-strategy position information
    struct alignas(64) NetPosition {
        int64_t total_position = 0;                     // Net position across all strategies (8 bytes)
        double total_unrealized_pnl = 0.0;              // Combined unrealized P&L (8 bytes)
        double total_daily_pnl = 0.0;                   // Combined daily P&L (8 bytes)
        uint32_t active_strategies = 0;                 // Number of strategies with positions (4 bytes)
        uint32_t _reserved = 0;                         // Reserved (4 bytes)
        uint64_t last_update_time_ns = 0;               // Last netting update time (8 bytes)
        uint8_t _pad[24];                               // Padding to 64 bytes (24 bytes)
        // Total: 8 + 8 + 8 + 4 + 4 + 8 + 24 = 64 bytes
        
        NetPosition() noexcept : _pad{} {}
    };
    static_assert(sizeof(NetPosition) == 64, "NetPosition must be 64 bytes");
    
    // Strategy coordination message
    struct CoordinationMessage {
        enum class Type : uint8_t {
            MARKET_UPDATE = 0,
            POSITION_UPDATE = 1,
            RISK_UPDATE = 2,
            CONFIG_UPDATE = 3,
            EMERGENCY_STOP = 4
        };
        
        Type type;
        uint8_t source_strategy_id;
        uint8_t target_strategy_id;  // 0xFF for broadcast
        uint8_t _pad[5];
        uint64_t timestamp_ns;
        
        union {
            SimpleMarketMaker::MarketUpdate market_update;
            struct {
                TreasuryType instrument;
                int64_t position_change;
                Price32nd fill_price;
            } position_update;
            
            struct {
                TreasuryType instrument;
                uint32_t risk_score;
                double portfolio_var;
            } risk_update;
            
            StrategyConfig config_update;
        } data;
        
        CoordinationMessage() noexcept : _pad{} {}
    };
    
    /**
     * @brief Constructor with object pools and infrastructure
     * @param order_pool Object pool for order allocation
     * @param level_pool Object pool for order book levels
     * @param update_buffer Ring buffer for order book updates
     * @param order_book Order book reference
     */
    StrategyCoordinator(
        hft::ObjectPool<TreasuryOrder, 4096>& order_pool,
        hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool,
        hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer,
        TreasuryOrderBook& order_book
    ) noexcept
        : order_pool_(order_pool),
          level_pool_(level_pool),
          update_buffer_(update_buffer),
          order_book_(order_book),
          message_buffer_(message_buffer_storage_),
          net_positions_{},
          strategy_configs_{},
          coordination_count_(0),
          total_coordination_time_ns_(0) {
        
        // Initialize strategies using tuple and index sequence
        init_strategies(std::index_sequence_for<StrategyTypes...>{});
        
        // Initialize net positions
        for (auto& pos : net_positions_) {
            pos.total_position = 0;
            pos.total_unrealized_pnl = 0.0;
            pos.total_daily_pnl = 0.0;
            pos.active_strategies = 0;
            pos.last_update_time_ns = 0;
        }
        
        // Initialize strategy configurations with default priorities
        init_strategy_configs(std::index_sequence_for<StrategyTypes...>{});
    }
    
    // No copy or move semantics for performance
    StrategyCoordinator(const StrategyCoordinator&) = delete;
    StrategyCoordinator& operator=(const StrategyCoordinator&) = delete;
    
    /**
     * @brief Coordinate all strategies for a market update
     * @param update Market data update
     * @return Array of strategy results ordered by priority
     */
    [[nodiscard]] std::array<StrategyResult, MAX_STRATEGIES> coordinate_strategies(
        const SimpleMarketMaker::MarketUpdate& update
    ) noexcept;
    
    /**
     * @brief Update strategy configuration
     * @param strategy_id Strategy index (0-based)
     * @param config New configuration
     * @return true if successful
     */
    bool update_strategy_config(uint8_t strategy_id, const StrategyConfig& config) noexcept;
    
    /**
     * @brief Get net position for instrument across all strategies
     * @param instrument Treasury instrument type
     * @return Net position information
     */
    [[nodiscard]] const NetPosition& get_net_position(TreasuryType instrument) const noexcept {
        const auto index = static_cast<size_t>(instrument);
        return (index < net_positions_.size()) ? net_positions_[index] : net_positions_[0];
    }
    
    /**
     * @brief Get average coordination overhead in nanoseconds
     * @return Average coordination latency
     */
    [[nodiscard]] uint64_t get_average_coordination_latency_ns() const noexcept {
        return coordination_count_ > 0 ? total_coordination_time_ns_ / coordination_count_ : 0;
    }
    
    /**
     * @brief Get strategy configuration
     * @param strategy_id Strategy index
     * @return Strategy configuration
     */
    [[nodiscard]] const StrategyConfig& get_strategy_config(uint8_t strategy_id) const noexcept {
        return (strategy_id < MAX_STRATEGIES) ? strategy_configs_[strategy_id] : strategy_configs_[0];
    }
    
    /**
     * @brief Emergency stop all strategies
     */
    void emergency_stop() noexcept;
    
    /**
     * @brief Update cross-strategy position netting
     * @param instrument Treasury instrument type
     */
    void update_position_netting(TreasuryType instrument) noexcept;

private:
    // Infrastructure references - cache-aligned
    alignas(64) hft::ObjectPool<TreasuryOrder, 4096>& order_pool_;
    alignas(64) hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool_;
    alignas(64) hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer_;
    alignas(64) TreasuryOrderBook& order_book_;
    alignas(64) hft::HFTTimer timer_;
    
    // Strategy instances stored in tuple for type safety
    std::tuple<StrategyTypes...> strategies_;
    
    // Inter-strategy communication
    alignas(64) CoordinationMessage message_buffer_storage_[MESSAGE_BUFFER_SIZE];
    hft::SPSCRingBuffer<CoordinationMessage, MESSAGE_BUFFER_SIZE> message_buffer_;
    
    // Cross-strategy position tracking
    alignas(64) std::array<NetPosition, MAX_INSTRUMENTS> net_positions_;
    
    // Strategy configurations
    alignas(64) std::array<StrategyConfig, MAX_STRATEGIES> strategy_configs_;
    
    // Performance tracking
    alignas(64) std::atomic<uint64_t> coordination_count_;
    alignas(64) std::atomic<uint64_t> total_coordination_time_ns_;
    
    // Template metaprogramming helpers for strategy initialization
    template<std::size_t... Is>
    void init_strategies(std::index_sequence<Is...>) noexcept {
        ((std::get<Is>(strategies_) = StrategyTypes(order_pool_, order_book_)), ...);
    }
    
    template<std::size_t... Is>
    void init_strategy_configs(std::index_sequence<Is...>) noexcept {
        ((strategy_configs_[Is] = StrategyConfig(static_cast<uint8_t>(Is))), ...);
    }
    
    // Template-based strategy execution
    template<std::size_t I>
    StrategyResult execute_strategy(const SimpleMarketMaker::MarketUpdate& update) noexcept {
        if constexpr (I < MAX_STRATEGIES) {
            const auto start_time = timer_.get_timestamp_ns();
            
            auto& strategy = std::get<I>(strategies_);
            const auto& config = strategy_configs_[I];
            
            StrategyResult result;
            result.strategy_id = static_cast<uint8_t>(I);
            result.priority = config.priority;
            
            if (!config.enabled) {
                result.action = StrategyResult::Action::NO_ACTION;
                result.execution_time_ns = timer_.get_timestamp_ns() - start_time;
                return result;
            }
            
            // Execute strategy decision
            auto decision = strategy.make_decision(update);
            
            // Convert to coordination result
            switch (decision.action) {
                case SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES:
                    result.action = StrategyResult::Action::UPDATE_QUOTES;
                    result.instrument = decision.instrument;
                    result.bid_price = decision.bid_price;
                    result.ask_price = decision.ask_price;
                    result.bid_size = static_cast<uint64_t>(decision.bid_size * config.resource_allocation);
                    result.ask_size = static_cast<uint64_t>(decision.ask_size * config.resource_allocation);
                    break;
                    
                case SimpleMarketMaker::TradingDecision::Action::CANCEL_QUOTES:
                    result.action = StrategyResult::Action::CANCEL_QUOTES;
                    result.instrument = decision.instrument;
                    break;
                    
                default:
                    result.action = StrategyResult::Action::NO_ACTION;
                    break;
            }
            
            result.execution_time_ns = timer_.get_timestamp_ns() - start_time;
            return result;
        } else {
            return StrategyResult{};
        }
    }
    
    // Recursively execute all strategies
    template<std::size_t I = 0>
    void execute_all_strategies(
        const SimpleMarketMaker::MarketUpdate& update,
        std::array<StrategyResult, MAX_STRATEGIES>& results
    ) noexcept {
        if constexpr (I < MAX_STRATEGIES) {
            results[I] = execute_strategy<I>(update);
            execute_all_strategies<I + 1>(update, results);
        }
    }
    
    // Sort results by priority (stable sort to maintain order for same priority)
    void sort_results_by_priority(std::array<StrategyResult, MAX_STRATEGIES>& results) noexcept;
    
    // Check cross-strategy risk limits
    bool check_cross_strategy_risk_limits(const StrategyResult& result) noexcept;
    
    // Helper for position aggregation across strategies
    template<std::size_t I>
    void aggregate_positions(TreasuryType instrument, NetPosition& net_pos) noexcept;
    
    // Helper for risk score checking with template dispatch
    template<std::size_t I>
    bool check_strategy_risk_score(uint8_t strategy_id, TreasuryType instrument, uint32_t limit) noexcept;
};

// Implementation of key methods

template<typename... StrategyTypes>
inline std::array<typename StrategyCoordinator<StrategyTypes...>::StrategyResult, StrategyCoordinator<StrategyTypes...>::MAX_STRATEGIES>
StrategyCoordinator<StrategyTypes...>::coordinate_strategies(
    const SimpleMarketMaker::MarketUpdate& update
) noexcept {
    const auto coordination_start = timer_.get_timestamp_ns();
    
    std::array<StrategyResult, MAX_STRATEGIES> results;
    
    // Execute all strategies in parallel (template unrolling)
    execute_all_strategies(update, results);
    
    // Sort by priority for conflict resolution
    sort_results_by_priority(results);
    
    // Apply cross-strategy risk checks and position netting
    for (auto& result : results) {
        if (result.action == StrategyResult::Action::UPDATE_QUOTES ||
            result.action == StrategyResult::Action::CANCEL_QUOTES) {
            
            if (!check_cross_strategy_risk_limits(result)) {
                result.action = StrategyResult::Action::RISK_LIMIT_HIT;
            }
        }
        
        result.coordination_overhead_ns = timer_.get_timestamp_ns() - coordination_start;
    }
    
    // Update position netting for the instrument
    update_position_netting(update.instrument);
    
    // Update performance tracking
    const auto coordination_time = timer_.get_timestamp_ns() - coordination_start;
    coordination_count_.fetch_add(1, std::memory_order_relaxed);
    total_coordination_time_ns_.fetch_add(coordination_time, std::memory_order_relaxed);
    
    return results;
}

template<typename... StrategyTypes>
inline bool StrategyCoordinator<StrategyTypes...>::update_strategy_config(
    uint8_t strategy_id, 
    const StrategyConfig& config
) noexcept {
    if (strategy_id >= MAX_STRATEGIES) {
        return false;
    }
    
    strategy_configs_[strategy_id] = config;
    
    // Send configuration update message
    CoordinationMessage msg;
    msg.type = CoordinationMessage::Type::CONFIG_UPDATE;
    msg.source_strategy_id = 0xFF; // System
    msg.target_strategy_id = strategy_id;
    msg.timestamp_ns = timer_.get_timestamp_ns();
    msg.data.config_update = config;
    
    message_buffer_.push(msg);
    
    return true;
}

template<typename... StrategyTypes>
inline void StrategyCoordinator<StrategyTypes...>::emergency_stop() noexcept {
    // Disable all strategies immediately
    for (auto& config : strategy_configs_) {
        config.enabled = false;
    }
    
    // Send emergency stop messages
    CoordinationMessage msg;
    msg.type = CoordinationMessage::Type::EMERGENCY_STOP;
    msg.source_strategy_id = 0xFF; // System
    msg.target_strategy_id = 0xFF; // Broadcast
    msg.timestamp_ns = timer_.get_timestamp_ns();
    
    message_buffer_.push(msg);
}

template<typename... StrategyTypes>
inline void StrategyCoordinator<StrategyTypes...>::update_position_netting(TreasuryType instrument) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= net_positions_.size()) {
        return;
    }
    
    auto& net_pos = net_positions_[instrument_index];
    
    // Reset aggregated values
    net_pos.total_position = 0;
    net_pos.total_unrealized_pnl = 0.0;
    net_pos.total_daily_pnl = 0.0;
    net_pos.active_strategies = 0;
    
    // Aggregate across all strategies using template iteration
    aggregate_positions<0>(instrument, net_pos);
    
    net_pos.last_update_time_ns = timer_.get_timestamp_ns();
}

// Helper for position aggregation across strategies
template<typename... StrategyTypes>
template<std::size_t I>
void StrategyCoordinator<StrategyTypes...>::aggregate_positions(
    TreasuryType instrument, 
    NetPosition& net_pos
) noexcept {
    if constexpr (I < MAX_STRATEGIES) {
        auto& strategy = std::get<I>(strategies_);
        
        const int64_t position = strategy.get_position(instrument);
        const double unrealized_pnl = strategy.get_unrealized_pnl(instrument);
        const double daily_pnl = strategy.get_daily_pnl(instrument);
        
        net_pos.total_position += position;
        net_pos.total_unrealized_pnl += unrealized_pnl;
        net_pos.total_daily_pnl += daily_pnl;
        
        if (std::abs(position) > 1000) { // Minimum position threshold
            ++net_pos.active_strategies;
        }
        
        aggregate_positions<I + 1>(instrument, net_pos);
    }
}

template<typename... StrategyTypes>
inline void StrategyCoordinator<StrategyTypes...>::sort_results_by_priority(
    std::array<StrategyResult, MAX_STRATEGIES>& results
) noexcept {
    // Simple insertion sort optimized for small arrays (MAX_STRATEGIES typically ≤ 8)
    for (size_t i = 1; i < MAX_STRATEGIES; ++i) {
        StrategyResult key = results[i];
        size_t j = i;
        
        while (j > 0 && results[j - 1].priority > key.priority) {
            results[j] = results[j - 1];
            --j;
        }
        
        results[j] = key;
    }
}

template<typename... StrategyTypes>
inline bool StrategyCoordinator<StrategyTypes...>::check_cross_strategy_risk_limits(
    const StrategyResult& result
) noexcept {
    const auto& net_pos = get_net_position(result.instrument);
    const auto& config = get_strategy_config(result.strategy_id);
    
    // Check if net position would exceed limits
    const int64_t proposed_position_change = static_cast<int64_t>(result.bid_size + result.ask_size);
    const int64_t new_net_position = net_pos.total_position + proposed_position_change;
    
    if (std::abs(new_net_position) > static_cast<int64_t>(config.max_position_size)) {
        return false;
    }
    
    // Check strategy-specific risk score using template dispatch
    return check_strategy_risk_score<0>(result.strategy_id, result.instrument, config.risk_score_limit);
}

// Helper for risk score checking with template dispatch
template<typename... StrategyTypes>
template<std::size_t I>
bool StrategyCoordinator<StrategyTypes...>::check_strategy_risk_score(
    uint8_t strategy_id, 
    TreasuryType instrument, 
    uint32_t limit
) noexcept {
    if constexpr (I < MAX_STRATEGIES) {
        if (I == strategy_id) {
            auto& strategy = std::get<I>(strategies_);
            return strategy.get_risk_score(instrument) <= limit;
        } else {
            return check_strategy_risk_score<I + 1>(strategy_id, instrument, limit);
        }
    } else {
        return true; // Strategy not found, allow by default
    }
}

// Static asserts for complete type validation
static_assert(alignof(StrategyCoordinator<SimpleMarketMaker>) == 64, 
              "StrategyCoordinator must be cache-aligned");

} // namespace strategy
} // namespace hft