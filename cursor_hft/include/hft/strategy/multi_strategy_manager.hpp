#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/strategy/simple_market_maker.hpp"

namespace hft {
namespace strategy {

using namespace hft::market_data;

/**
 * @brief Multi-strategy management with sub-100ns coordination
 * 
 * Manages multiple SimpleMarketMaker strategies with:
 * - Priority-based execution order
 * - Resource allocation per strategy  
 * - Cross-strategy position netting
 * - Performance monitoring
 * 
 * Performance targets:
 * - Strategy coordination: <100ns overhead
 * - Strategy dispatch: <50ns per strategy
 * - Position netting: <200ns total
 */
class alignas(64) MultiStrategyManager {
public:
    static constexpr size_t MAX_STRATEGIES = 4;
    static constexpr size_t MAX_INSTRUMENTS = 6;
    
    // Strategy execution result
    struct alignas(64) StrategyResult {
        enum class Action : uint8_t {
            NO_ACTION = 0,
            UPDATE_QUOTES = 1,
            CANCEL_QUOTES = 2,
            RISK_LIMIT_HIT = 3
        };
        
        Action action = Action::NO_ACTION;              // 1 byte
        uint8_t strategy_id = 0;                        // 1 byte
        uint8_t priority = 0;                           // 1 byte
        uint8_t _pad0[5];                               // 5 bytes padding
        TreasuryType instrument;                        // 1 byte
        uint8_t _pad1[3];                               // 3 bytes padding
        uint32_t _reserved = 0;                         // 4 bytes
        Price32nd bid_price;                            // 8 bytes
        Price32nd ask_price;                            // 8 bytes
        uint64_t bid_size = 0;                          // 8 bytes
        uint64_t ask_size = 0;                          // 8 bytes
        uint64_t execution_time_ns = 0;                 // 8 bytes
        uint64_t coordination_overhead_ns = 0;          // 8 bytes
        
        StrategyResult() noexcept : _pad0{}, _pad1{} {}
    };
    static_assert(sizeof(StrategyResult) == 64, "StrategyResult must be 64 bytes");
    
    // Strategy configuration
    struct StrategyConfig {
        uint8_t priority = 0;                           // 0 = highest priority
        bool enabled = true;                            // Strategy enabled/disabled
        double resource_allocation = 1.0;               // Resource allocation factor (0.0-1.0)
        uint64_t max_position_size = 50000000;          // Per-strategy position limit
        uint32_t risk_score_limit = 800;                // Risk score threshold
        
        StrategyConfig() noexcept = default;
        StrategyConfig(uint8_t prio, bool en = true, double alloc = 1.0) noexcept
            : priority(prio), enabled(en), resource_allocation(alloc) {}
    };
    
    // Net position across strategies
    struct alignas(64) NetPosition {
        int64_t total_position = 0;                     // Net position across strategies (8 bytes)
        double total_unrealized_pnl = 0.0;              // Combined unrealized P&L (8 bytes)
        double total_daily_pnl = 0.0;                   // Combined daily P&L (8 bytes)
        uint32_t active_strategies = 0;                 // Strategies with positions (4 bytes)
        uint32_t _reserved = 0;                         // Reserved (4 bytes)
        uint64_t last_update_time_ns = 0;               // Last update time (8 bytes)
        uint8_t _pad[24];                               // Padding to 64 bytes
        
        NetPosition() noexcept : _pad{} {}
    };
    static_assert(sizeof(NetPosition) == 64, "NetPosition must be 64 bytes");
    
    /**
     * @brief Constructor with infrastructure dependencies
     */
    MultiStrategyManager(
        hft::ObjectPool<TreasuryOrder, 4096>& order_pool,
        hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool,
        hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer,
        TreasuryOrderBook& order_book
    ) noexcept;
    
    // No copy or move semantics
    MultiStrategyManager(const MultiStrategyManager&) = delete;
    MultiStrategyManager& operator=(const MultiStrategyManager&) = delete;
    
    /**
     * @brief Coordinate all active strategies for market update
     * @param update Market data update
     * @return Array of strategy results ordered by priority
     */
    [[nodiscard]] std::array<StrategyResult, MAX_STRATEGIES> coordinate_strategies(
        const SimpleMarketMaker::MarketUpdate& update
    ) noexcept;
    
    /**
     * @brief Update strategy configuration
     * @param strategy_id Strategy index (0-3)
     * @param config New configuration
     * @return true if successful
     */
    bool update_strategy_config(uint8_t strategy_id, const StrategyConfig& config) noexcept;
    
    /**
     * @brief Get strategy configuration
     * @param strategy_id Strategy index
     * @return Strategy configuration
     */
    [[nodiscard]] const StrategyConfig& get_strategy_config(uint8_t strategy_id) const noexcept;
    
    /**
     * @brief Get net position for instrument
     * @param instrument Treasury instrument type
     * @return Net position information
     */
    [[nodiscard]] const NetPosition& get_net_position(TreasuryType instrument) const noexcept;
    
    /**
     * @brief Get average coordination latency
     * @return Average coordination overhead in nanoseconds
     */
    [[nodiscard]] uint64_t get_average_coordination_latency_ns() const noexcept {
        return coordination_count_ > 0 ? total_coordination_time_ns_ / coordination_count_ : 0;
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
    // Infrastructure references
    alignas(64) hft::ObjectPool<TreasuryOrder, 4096>& order_pool_;
    alignas(64) hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool_;
    alignas(64) hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer_;
    alignas(64) TreasuryOrderBook& order_book_;
    alignas(64) hft::HFTTimer timer_;
    
    // Strategy instances (fixed size for performance)
    alignas(64) std::array<std::unique_ptr<SimpleMarketMaker>, MAX_STRATEGIES> strategies_;
    
    // Strategy configurations
    alignas(64) std::array<StrategyConfig, MAX_STRATEGIES> strategy_configs_;
    
    // Cross-strategy position tracking
    alignas(64) std::array<NetPosition, MAX_INSTRUMENTS> net_positions_;
    
    // Performance tracking
    alignas(64) std::atomic<uint64_t> coordination_count_;
    alignas(64) std::atomic<uint64_t> total_coordination_time_ns_;
    
    // Helper methods
    void sort_results_by_priority(std::array<StrategyResult, MAX_STRATEGIES>& results) noexcept;
    bool check_cross_strategy_risk_limits(const StrategyResult& result) noexcept;
};

// Implementation

inline MultiStrategyManager::MultiStrategyManager(
    hft::ObjectPool<TreasuryOrder, 4096>& order_pool,
    hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>& level_pool,
    hft::SPSCRingBuffer<OrderBookUpdate, 8192>& update_buffer,
    TreasuryOrderBook& order_book
) noexcept
    : order_pool_(order_pool),
      level_pool_(level_pool),
      update_buffer_(update_buffer),
      order_book_(order_book),
      timer_(),
      strategies_{},
      strategy_configs_{},
      net_positions_{},
      coordination_count_(0),
      total_coordination_time_ns_(0) {
    
    // Initialize strategies
    for (size_t i = 0; i < MAX_STRATEGIES; ++i) {
        strategies_[i] = std::make_unique<SimpleMarketMaker>(order_pool_, order_book_);
        strategy_configs_[i] = StrategyConfig(static_cast<uint8_t>(i));
    }
    
    // Initialize net positions
    for (auto& pos : net_positions_) {
        pos.total_position = 0;
        pos.total_unrealized_pnl = 0.0;
        pos.total_daily_pnl = 0.0;
        pos.active_strategies = 0;
        pos.last_update_time_ns = 0;
    }
}

inline std::array<MultiStrategyManager::StrategyResult, MultiStrategyManager::MAX_STRATEGIES>
MultiStrategyManager::coordinate_strategies(
    const SimpleMarketMaker::MarketUpdate& update
) noexcept {
    const auto coordination_start = timer_.get_timestamp_ns();
    
    std::array<StrategyResult, MAX_STRATEGIES> results;
    
    // Execute each strategy
    for (size_t i = 0; i < MAX_STRATEGIES; ++i) {
        const auto start_time = timer_.get_timestamp_ns();
        
        auto& result = results[i];
        result.strategy_id = static_cast<uint8_t>(i);
        result.priority = strategy_configs_[i].priority;
        
        if (!strategy_configs_[i].enabled) {
            result.action = StrategyResult::Action::NO_ACTION;
            result.execution_time_ns = timer_.get_timestamp_ns() - start_time;
            continue;
        }
        
        // Execute strategy decision
        auto decision = strategies_[i]->make_decision(update);
        
        // Convert to coordination result
        result.instrument = decision.instrument;
        switch (decision.action) {
            case SimpleMarketMaker::TradingDecision::Action::UPDATE_QUOTES:
                result.action = StrategyResult::Action::UPDATE_QUOTES;
                result.bid_price = decision.bid_price;
                result.ask_price = decision.ask_price;
                result.bid_size = static_cast<uint64_t>(decision.bid_size * strategy_configs_[i].resource_allocation);
                result.ask_size = static_cast<uint64_t>(decision.ask_size * strategy_configs_[i].resource_allocation);
                break;
                
            case SimpleMarketMaker::TradingDecision::Action::CANCEL_QUOTES:
                result.action = StrategyResult::Action::CANCEL_QUOTES;
                break;
                
            default:
                result.action = StrategyResult::Action::NO_ACTION;
                break;
        }
        
        result.execution_time_ns = timer_.get_timestamp_ns() - start_time;
    }
    
    // Sort results by priority
    sort_results_by_priority(results);
    
    // Apply cross-strategy risk checks
    for (auto& result : results) {
        if (result.action == StrategyResult::Action::UPDATE_QUOTES) {
            if (!check_cross_strategy_risk_limits(result)) {
                result.action = StrategyResult::Action::RISK_LIMIT_HIT;
            }
        }
        
        result.coordination_overhead_ns = timer_.get_timestamp_ns() - coordination_start;
    }
    
    // Update position netting
    update_position_netting(update.instrument);
    
    // Update performance tracking
    const auto coordination_time = timer_.get_timestamp_ns() - coordination_start;
    coordination_count_.fetch_add(1, std::memory_order_relaxed);
    total_coordination_time_ns_.fetch_add(coordination_time, std::memory_order_relaxed);
    
    return results;
}

inline bool MultiStrategyManager::update_strategy_config(
    uint8_t strategy_id, 
    const StrategyConfig& config
) noexcept {
    if (strategy_id >= MAX_STRATEGIES) {
        return false;
    }
    
    strategy_configs_[strategy_id] = config;
    return true;
}

inline const MultiStrategyManager::StrategyConfig& 
MultiStrategyManager::get_strategy_config(uint8_t strategy_id) const noexcept {
    return (strategy_id < MAX_STRATEGIES) ? strategy_configs_[strategy_id] : strategy_configs_[0];
}

inline const MultiStrategyManager::NetPosition& 
MultiStrategyManager::get_net_position(TreasuryType instrument) const noexcept {
    const auto index = static_cast<size_t>(instrument);
    return (index < net_positions_.size()) ? net_positions_[index] : net_positions_[0];
}

inline void MultiStrategyManager::emergency_stop() noexcept {
    for (auto& config : strategy_configs_) {
        config.enabled = false;
    }
}

inline void MultiStrategyManager::update_position_netting(TreasuryType instrument) noexcept {
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
    
    // Aggregate across all strategies
    for (size_t i = 0; i < MAX_STRATEGIES; ++i) {
        if (!strategies_[i] || !strategy_configs_[i].enabled) {
            continue;
        }
        
        const int64_t position = strategies_[i]->get_position(instrument);
        const double unrealized_pnl = strategies_[i]->get_unrealized_pnl(instrument);
        const double daily_pnl = strategies_[i]->get_daily_pnl(instrument);
        
        net_pos.total_position += position;
        net_pos.total_unrealized_pnl += unrealized_pnl;
        net_pos.total_daily_pnl += daily_pnl;
        
        if (std::abs(position) > 1000) { // Minimum position threshold
            ++net_pos.active_strategies;
        }
    }
    
    net_pos.last_update_time_ns = timer_.get_timestamp_ns();
}

inline void MultiStrategyManager::sort_results_by_priority(
    std::array<StrategyResult, MAX_STRATEGIES>& results
) noexcept {
    // Simple insertion sort for small fixed array
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

inline bool MultiStrategyManager::check_cross_strategy_risk_limits(
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
    
    // Check strategy-specific risk score
    if (result.strategy_id < MAX_STRATEGIES && strategies_[result.strategy_id]) {
        const uint32_t risk_score = strategies_[result.strategy_id]->get_risk_score(result.instrument);
        if (risk_score > config.risk_score_limit) {
            return false;
        }
    }
    
    return true;
}

// Static asserts
static_assert(alignof(MultiStrategyManager) == 64, "MultiStrategyManager must be cache-aligned");

} // namespace strategy
} // namespace hft