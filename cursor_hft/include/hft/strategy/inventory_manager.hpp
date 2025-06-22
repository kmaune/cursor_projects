#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <utility>
#include <type_traits>
#include <cassert>

#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/market_data/treasury_instruments.hpp"

// Forward declare to access common constants without circular includes
namespace hft { namespace strategy {
    extern const uint32_t MAX_POSITION_SIZE;
    extern const uint32_t DV01_PORTFOLIO_LIMIT; 
    extern const std::array<double, 6> DV01_PER_MILLION;
}}

namespace hft {
namespace strategy {

using namespace hft::market_data;

// Use common constants to avoid redefinition - qualify with anonymous namespace

// Position and risk limits (configurable) - avoid redefining constants from integrated strategy
constexpr int64_t MAX_PORTFOLIO_NOTIONAL = 500000000;   // $500M total notional
constexpr double MAX_INSTRUMENT_DV01 = 20000.0;         // $20k per instrument DV01
constexpr double CONCENTRATION_LIMIT = 0.6;             // Max 60% of portfolio in one instrument

// Inventory penalty parameters
constexpr double BASE_INVENTORY_PENALTY_BPS = 0.5;      // 0.5bp per $10M inventory
constexpr double CONCENTRATION_PENALTY_BPS = 1.0;       // Additional 1bp when concentrated
constexpr double TIME_DECAY_FACTOR = 0.8;               // 80% penalty reduction per hour
constexpr double END_OF_DAY_PENALTY_MULTIPLIER = 3.0;   // 3x penalty in last hour

// Years to maturity for yield curve calculations
constexpr std::array<double, 6> YEARS_TO_MATURITY = {
    0.25,   // 3M Bill
    0.5,    // 6M Bill
    2.0,    // 2Y Note
    5.0,    // 5Y Note
    10.0,   // 10Y Note
    30.0    // 30Y Bond
};

// Position state for a single instrument
struct alignas(64) InstrumentPosition {
    TreasuryType instrument;                           // 1 byte
    uint8_t _pad0[7];                                 // 7 bytes padding
    
    // Position tracking
    std::atomic<int64_t> net_position;               // 8 bytes - net position in cents
    std::atomic<int64_t> gross_position;             // 8 bytes - gross position (abs value)
    std::atomic<double> average_price;               // 8 bytes - VWAP of position
    std::atomic<double> unrealized_pnl;              // 8 bytes - Mark-to-market P&L
    
    // Risk metrics
    std::atomic<double> position_dv01;               // 8 bytes - DV01 exposure
    std::atomic<double> duration;                    // 8 bytes - Modified duration
    
    // Timing and tracking
    std::atomic<hft::HFTTimer::timestamp_t> last_trade_time; // 8 bytes
    std::atomic<uint32_t> trade_count_today;         // 4 bytes
    uint8_t _pad1[4];                               // 4 bytes padding
    // Total: 64 bytes exactly
    
    InstrumentPosition() noexcept : instrument(TreasuryType::Note_10Y) {
        std::memset(_pad0, 0, sizeof(_pad0));
        std::memset(_pad1, 0, sizeof(_pad1));
        net_position.store(0, std::memory_order_relaxed);
        gross_position.store(0, std::memory_order_relaxed);
        average_price.store(0.0, std::memory_order_relaxed);
        unrealized_pnl.store(0.0, std::memory_order_relaxed);
        position_dv01.store(0.0, std::memory_order_relaxed);
        duration.store(0.0, std::memory_order_relaxed);
        last_trade_time.store(0, std::memory_order_relaxed);
        trade_count_today.store(0, std::memory_order_relaxed);
    }
    
    explicit InstrumentPosition(TreasuryType instr) noexcept : InstrumentPosition() {
        instrument = instr;
    }
};

// Portfolio-level risk metrics
struct alignas(64) PortfolioRiskState {
    // Aggregate metrics
    std::atomic<int64_t> total_notional;             // 8 bytes - total absolute notional
    std::atomic<double> portfolio_dv01;              // 8 bytes - total portfolio DV01
    std::atomic<double> portfolio_duration;          // 8 bytes - portfolio duration
    std::atomic<double> realized_pnl_today;          // 8 bytes - realized P&L today
    std::atomic<double> unrealized_pnl;              // 8 bytes - current unrealized P&L
    
    // Risk utilization
    std::atomic<double> position_limit_utilization;  // 8 bytes - % of position limit used
    std::atomic<double> dv01_limit_utilization;      // 8 bytes - % of DV01 limit used
    std::atomic<double> concentration_ratio;         // 8 bytes - largest position / total
    
    // State tracking
    std::atomic<uint64_t> state_version;             // 8 bytes - for consistency checking
    uint8_t _pad[8];                                // 8 bytes padding
    // Total: 64 bytes exactly
    
    PortfolioRiskState() noexcept {
        std::memset(_pad, 0, sizeof(_pad));
        total_notional.store(0, std::memory_order_relaxed);
        portfolio_dv01.store(0.0, std::memory_order_relaxed);
        portfolio_duration.store(0.0, std::memory_order_relaxed);
        realized_pnl_today.store(0.0, std::memory_order_relaxed);
        unrealized_pnl.store(0.0, std::memory_order_relaxed);
        position_limit_utilization.store(0.0, std::memory_order_relaxed);
        dv01_limit_utilization.store(0.0, std::memory_order_relaxed);
        concentration_ratio.store(0.0, std::memory_order_relaxed);
        state_version.store(1, std::memory_order_relaxed);
    }
};

// Trade execution record for position updates
struct TradeExecution {
    TreasuryType instrument;
    int64_t quantity;                  // Signed: positive = long, negative = short
    Price32nd execution_price;
    hft::HFTTimer::timestamp_t execution_time;
    uint64_t trade_id;
    
    TradeExecution() noexcept = default;
    
    TradeExecution(TreasuryType instr, int64_t qty, Price32nd price) noexcept
        : instrument(instr), quantity(qty), execution_price(price),
          execution_time(hft::HFTTimer::get_timestamp_ns()), trade_id(0) {}
};

// Inventory adjustment calculation result
struct InventoryAdjustment {
    double bid_adjustment_bps;         // Adjustment to bid price (negative = wider)
    double ask_adjustment_bps;         // Adjustment to ask price (positive = wider)
    double size_multiplier;            // Size reduction factor (0.0 to 1.0)
    bool should_stop_quoting;          // Emergency position control
    
    InventoryAdjustment() noexcept 
        : bid_adjustment_bps(0.0), ask_adjustment_bps(0.0), 
          size_multiplier(1.0), should_stop_quoting(false) {}
};

// Main inventory manager class
class InventoryManager {
private:
    // Position state per instrument (cache-aligned)
    alignas(64) std::array<InstrumentPosition, 6> positions_;
    alignas(64) PortfolioRiskState portfolio_state_;
    
    // Configuration parameters
    double inventory_penalty_bps_;
    double concentration_penalty_bps_;
    double max_position_size_;
    double max_portfolio_dv01_;
    
    // Performance tracking
    hft::HFTTimer timer_;
    mutable std::atomic<uint64_t> position_updates_processed_;
    mutable std::atomic<uint64_t> total_update_latency_ns_;
    
public:
    InventoryManager() noexcept
        : inventory_penalty_bps_(BASE_INVENTORY_PENALTY_BPS)
        , concentration_penalty_bps_(CONCENTRATION_PENALTY_BPS)
        , max_position_size_(MAX_POSITION_SIZE)
        , max_portfolio_dv01_(DV01_PORTFOLIO_LIMIT)
        , timer_()
        , position_updates_processed_(0)
        , total_update_latency_ns_(0) {
        
        // Initialize positions for each instrument (cannot assign due to atomic members)
        for (size_t i = 0; i < 6; ++i) {
            auto& pos = positions_[i];
            pos.instrument = static_cast<TreasuryType>(i);
            pos.net_position.store(0, std::memory_order_relaxed);
            pos.gross_position.store(0, std::memory_order_relaxed);
            pos.average_price.store(0.0, std::memory_order_relaxed);
            pos.unrealized_pnl.store(0.0, std::memory_order_relaxed);
            pos.position_dv01.store(0.0, std::memory_order_relaxed);
            pos.duration.store(0.0, std::memory_order_relaxed);
            pos.last_trade_time.store(0, std::memory_order_relaxed);
            pos.trade_count_today.store(0, std::memory_order_relaxed);
        }
    }
    
    // Delete copy/move constructors to ensure single instance
    InventoryManager(const InventoryManager&) = delete;
    InventoryManager& operator=(const InventoryManager&) = delete;
    InventoryManager(InventoryManager&&) = delete;
    InventoryManager& operator=(InventoryManager&&) = delete;
    
    // Update position after trade execution (target: 200ns)
    bool update_position(const TradeExecution& trade) noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        const auto instrument_index = static_cast<size_t>(trade.instrument);
        auto& position = positions_[instrument_index];
        
        // Update position atomically (target: 50ns)
        const auto old_position = position.net_position.load(std::memory_order_relaxed);
        const auto new_position = old_position + trade.quantity;
        position.net_position.store(new_position, std::memory_order_relaxed);
        
        // Update gross position (target: 20ns)
        position.gross_position.store(std::abs(new_position), std::memory_order_relaxed);
        
        // Update VWAP (target: 50ns)
        const auto current_price = position.average_price.load(std::memory_order_relaxed);
        if (old_position != 0 && new_position != 0) {
            // Weighted average price calculation
            const double old_value = old_position * current_price;
            const double new_value = trade.quantity * trade.execution_price.to_decimal();
            const double updated_avg = (old_value + new_value) / new_position;
            position.average_price.store(updated_avg, std::memory_order_relaxed);
        } else if (new_position != 0) {
            // New position
            position.average_price.store(trade.execution_price.to_decimal(), std::memory_order_relaxed);
        } else {
            // Flat position
            position.average_price.store(0.0, std::memory_order_relaxed);
        }
        
        // Update DV01 exposure (target: 30ns)
        const double notional_millions = std::abs(new_position) / 1000000.0;
        const double new_dv01 = notional_millions * DV01_PER_MILLION[instrument_index];
        position.position_dv01.store(new_dv01, std::memory_order_relaxed);
        
        // Update trade tracking (target: 20ns)
        position.last_trade_time.store(trade.execution_time, std::memory_order_relaxed);
        position.trade_count_today.fetch_add(1, std::memory_order_relaxed);
        
        // Update portfolio-level metrics (target: 30ns)
        update_portfolio_metrics();
        
        // Performance tracking
        const auto end_time = timer_.get_timestamp_ns();
        position_updates_processed_.fetch_add(1, std::memory_order_relaxed);
        total_update_latency_ns_.fetch_add(end_time - start_time, std::memory_order_relaxed);
        
        return true;
    }
    
    // Calculate inventory-based pricing adjustments (target: 100ns)
    InventoryAdjustment calculate_inventory_adjustment(TreasuryType instrument, 
                                                     double current_time_factor = 1.0) const noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        const auto& position = positions_[instrument_index];
        
        InventoryAdjustment adjustment;
        
        // Get current position (target: 10ns)
        const auto net_position = position.net_position.load(std::memory_order_relaxed);
        const auto position_dv01 = position.position_dv01.load(std::memory_order_relaxed);
        
        // Check emergency position limits (target: 20ns)
        if (std::abs(net_position) >= max_position_size_ * 0.95 || // 95% of limit
            position_dv01 >= MAX_INSTRUMENT_DV01 * 0.95) {
            adjustment.should_stop_quoting = true;
            return adjustment;
        }
        
        // Calculate base inventory penalty (target: 30ns)
        const double position_millions = net_position / 1000000.0; // Convert to millions
        const double base_penalty = (position_millions / 10.0) * inventory_penalty_bps_;
        
        // Apply time decay factor (target: 20ns)
        const double time_adjusted_penalty = base_penalty * current_time_factor;
        
        // Check concentration risk (target: 20ns)
        const auto total_notional = portfolio_state_.total_notional.load(std::memory_order_relaxed);
        const double concentration = total_notional > 0 ? 
            std::abs(net_position) / static_cast<double>(total_notional) : 0.0;
        
        double concentration_adjustment = 0.0;
        if (concentration > CONCENTRATION_LIMIT) {
            concentration_adjustment = (concentration - CONCENTRATION_LIMIT) * concentration_penalty_bps_;
        }
        
        // Calculate directional adjustments (target: 20ns)
        if (net_position > 0) { // Long position - penalize buying, favor selling
            adjustment.bid_adjustment_bps = -(time_adjusted_penalty + concentration_adjustment);
            adjustment.ask_adjustment_bps = time_adjusted_penalty * 0.5; // Smaller adjustment for selling
        } else if (net_position < 0) { // Short position - penalize selling, favor buying
            adjustment.bid_adjustment_bps = time_adjusted_penalty * 0.5; // Smaller adjustment for buying
            adjustment.ask_adjustment_bps = time_adjusted_penalty + concentration_adjustment;
        }
        // else: flat position, no adjustment needed
        
        // Calculate size multiplier based on position utilization
        const double position_utilization = std::abs(net_position) / max_position_size_;
        adjustment.size_multiplier = std::max(0.1, 1.0 - position_utilization);
        
        return adjustment;
    }
    
    // Check if new trade would violate risk limits (target: 50ns)
    bool can_trade(TreasuryType instrument, int64_t proposed_quantity) const noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        const auto& position = positions_[instrument_index];
        
        // Check position limits
        const auto current_position = position.net_position.load(std::memory_order_relaxed);
        const auto new_position = current_position + proposed_quantity;
        
        if (std::abs(new_position) > max_position_size_) {
            return false;
        }
        
        // Check DV01 limits
        const double new_notional_millions = std::abs(new_position) / 1000000.0;
        const double new_dv01 = new_notional_millions * DV01_PER_MILLION[instrument_index];
        
        if (new_dv01 > MAX_INSTRUMENT_DV01) {
            return false;
        }
        
        // Check portfolio DV01 limit
        const auto current_portfolio_dv01 = portfolio_state_.portfolio_dv01.load(std::memory_order_relaxed);
        const auto current_position_dv01 = position.position_dv01.load(std::memory_order_relaxed);
        const auto dv01_change = new_dv01 - current_position_dv01;
        
        if (current_portfolio_dv01 + dv01_change > max_portfolio_dv01_) {
            return false;
        }
        
        return true;
    }
    
    // Mark-to-market all positions with current prices (target: 300ns)
    void mark_to_market(const std::array<Price32nd, 6>& current_prices) noexcept {
        double total_unrealized_pnl = 0.0;
        
        for (size_t i = 0; i < 6; ++i) {
            auto& position = positions_[i];
            const auto net_position = position.net_position.load(std::memory_order_relaxed);
            
            if (net_position != 0) {
                const auto avg_price = position.average_price.load(std::memory_order_relaxed);
                const auto current_price = current_prices[i].to_decimal();
                const auto unrealized_pnl = net_position * (current_price - avg_price);
                
                position.unrealized_pnl.store(unrealized_pnl, std::memory_order_relaxed);
                total_unrealized_pnl += unrealized_pnl;
            } else {
                position.unrealized_pnl.store(0.0, std::memory_order_relaxed);
            }
        }
        
        portfolio_state_.unrealized_pnl.store(total_unrealized_pnl, std::memory_order_relaxed);
    }
    
    // Get current position for an instrument
    int64_t get_position(TreasuryType instrument) const noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        return positions_[instrument_index].net_position.load(std::memory_order_relaxed);
    }
    
    // Get current portfolio metrics
    const PortfolioRiskState& get_portfolio_state() const noexcept {
        return portfolio_state_; // Return reference to avoid copying atomic state
    }
    
    // Get detailed position information
    struct PositionInfo {
        TreasuryType instrument;
        int64_t net_position;
        int64_t gross_position;
        double average_price;
        double unrealized_pnl;
        double position_dv01;
        uint32_t trade_count_today;
        hft::HFTTimer::timestamp_t last_trade_time;
    };
    
    PositionInfo get_position_info(TreasuryType instrument) const noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        const auto& position = positions_[instrument_index];
        
        PositionInfo info{};
        info.instrument = instrument;
        info.net_position = position.net_position.load(std::memory_order_relaxed);
        info.gross_position = position.gross_position.load(std::memory_order_relaxed);
        info.average_price = position.average_price.load(std::memory_order_relaxed);
        info.unrealized_pnl = position.unrealized_pnl.load(std::memory_order_relaxed);
        info.position_dv01 = position.position_dv01.load(std::memory_order_relaxed);
        info.trade_count_today = position.trade_count_today.load(std::memory_order_relaxed);
        info.last_trade_time = position.last_trade_time.load(std::memory_order_relaxed);
        
        return info;
    }
    
    // Performance statistics
    struct InventoryManagerStats {
        uint64_t position_updates_processed;
        double average_update_latency_ns;
        std::array<int64_t, 6> current_positions;
        double total_portfolio_dv01;
        double total_unrealized_pnl;
        double position_limit_utilization;
        double dv01_limit_utilization;
    };
    
    InventoryManagerStats get_performance_stats() const noexcept {
        const auto total_updates = position_updates_processed_.load(std::memory_order_relaxed);
        const auto total_latency = total_update_latency_ns_.load(std::memory_order_relaxed);
        
        InventoryManagerStats stats{};
        stats.position_updates_processed = total_updates;
        stats.average_update_latency_ns = total_updates > 0 ? 
            static_cast<double>(total_latency) / total_updates : 0.0;
        
        for (size_t i = 0; i < 6; ++i) {
            stats.current_positions[i] = positions_[i].net_position.load(std::memory_order_relaxed);
        }
        
        stats.total_portfolio_dv01 = portfolio_state_.portfolio_dv01.load(std::memory_order_relaxed);
        stats.total_unrealized_pnl = portfolio_state_.unrealized_pnl.load(std::memory_order_relaxed);
        stats.position_limit_utilization = portfolio_state_.position_limit_utilization.load(std::memory_order_relaxed);
        stats.dv01_limit_utilization = portfolio_state_.dv01_limit_utilization.load(std::memory_order_relaxed);
        
        return stats;
    }
    
private:
    // Update portfolio-level risk metrics (called after position changes)
    void update_portfolio_metrics() noexcept {
        int64_t total_notional = 0;
        double total_dv01 = 0.0;
        double max_position = 0.0;
        
        for (size_t i = 0; i < 6; ++i) {
            const auto gross_position = positions_[i].gross_position.load(std::memory_order_relaxed);
            const auto position_dv01 = positions_[i].position_dv01.load(std::memory_order_relaxed);
            
            total_notional += gross_position;
            total_dv01 += position_dv01;
            max_position = std::max(max_position, static_cast<double>(gross_position));
        }
        
        // Update portfolio metrics
        portfolio_state_.total_notional.store(total_notional, std::memory_order_relaxed);
        portfolio_state_.portfolio_dv01.store(total_dv01, std::memory_order_relaxed);
        
        // Update utilization ratios
        const double position_util = total_notional / (MAX_PORTFOLIO_NOTIONAL * 1.0);
        const double dv01_util = total_dv01 / max_portfolio_dv01_;
        const double concentration = total_notional > 0 ? max_position / total_notional : 0.0;
        
        portfolio_state_.position_limit_utilization.store(position_util, std::memory_order_relaxed);
        portfolio_state_.dv01_limit_utilization.store(dv01_util, std::memory_order_relaxed);
        portfolio_state_.concentration_ratio.store(concentration, std::memory_order_relaxed);
        
        // Increment state version for consistency tracking
        portfolio_state_.state_version.fetch_add(1, std::memory_order_relaxed);
    }
};

} // namespace strategy
} // namespace hft