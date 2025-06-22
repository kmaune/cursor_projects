#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <array>
#include <utility>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <cassert>

#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/trading/order_book.hpp"

namespace hft {
namespace strategy {

using namespace hft::market_data;
using namespace hft::trading;

// ARM64 cache line size
constexpr size_t CACHE_LINE_SIZE = 64;

// Strategy performance targets
constexpr uint64_t STRATEGY_DECISION_TARGET_NS = 1200;  // 1.2μs total budget
constexpr uint64_t ESSENTIAL_PHASE_BUDGET_NS = 400;    // Phase 1: Essential analysis
constexpr uint64_t ENHANCED_PHASE_BUDGET_NS = 600;     // Phase 2: Enhanced analysis  
constexpr uint64_t QUOTE_PHASE_BUDGET_NS = 200;        // Phase 3: Quote generation

// Market making parameters
constexpr uint32_t MAX_POSITION_SIZE = 100000000;      // $100M position limit
constexpr uint32_t MAX_DAILY_LOSS = 1000000;           // $1M daily loss limit
constexpr uint32_t MAX_ORDER_FREQUENCY = 1000;         // 1000 orders/sec limit
constexpr uint32_t DV01_PORTFOLIO_LIMIT = 50000;       // $50k DV01 limit

// Treasury market constants
constexpr uint32_t TREASURY_32NDS_DENOMINATOR = 32;
constexpr double DEFAULT_SPREAD_BPS = 1.0;             // 1bp default spread
constexpr double INVENTORY_PENALTY_BPS = 0.5;          // 0.5bp per $10M inventory

// DV01 values per $1M notional (pre-computed for performance)
constexpr std::array<double, 6> DV01_PER_MILLION = {
    98.0,   // 3M Bill (approximated)
    196.0,  // 6M Bill (approximated)  
    196.0,  // 2Y Note
    472.0,  // 5Y Note
    867.0,  // 10Y Note
    1834.0  // 30Y Bond
};

// Market update structure for strategy input
struct alignas(CACHE_LINE_SIZE) MarketUpdate {
    TreasuryType instrument;                    // 1 byte
    uint8_t _pad0[7];                          // 7 bytes padding
    Price32nd best_bid;                        // 8 bytes
    Price32nd best_ask;                        // 8 bytes
    uint64_t bid_size;                         // 8 bytes
    uint64_t ask_size;                         // 8 bytes
    Price32nd last_trade_price;                // 8 bytes
    uint64_t last_trade_size;                  // 8 bytes
    hft::HFTTimer::timestamp_t timestamp_ns;   // 8 bytes
    uint64_t sequence_number;                  // 8 bytes
    uint8_t _pad1[8];                         // 8 bytes padding
    // Total: 64 bytes exactly
    
    MarketUpdate() noexcept = default;
    
    MarketUpdate(TreasuryType instr, Price32nd bid, Price32nd ask, 
                uint64_t bid_sz, uint64_t ask_sz) noexcept
        : instrument(instr), best_bid(bid), best_ask(ask), 
          bid_size(bid_sz), ask_size(ask_sz), 
          last_trade_price(Price32nd::from_decimal(0.0)), last_trade_size(0),
          timestamp_ns(hft::HFTTimer::get_timestamp_ns()), sequence_number(0) {
        std::memset(_pad0, 0, sizeof(_pad0));
        std::memset(_pad1, 0, sizeof(_pad1));
    }
};

// Trading decision output structure
struct alignas(CACHE_LINE_SIZE) TradingDecision {
    enum class Action : uint8_t {
        NO_ACTION = 0,
        UPDATE_QUOTES = 1,
        CANCEL_QUOTES = 2,
        EMERGENCY_FLATTEN = 3
    };
    
    Action action;                             // 1 byte
    TreasuryType instrument;                   // 1 byte
    uint8_t _pad0[6];                         // 6 bytes padding
    Price32nd bid_price;                       // 8 bytes
    Price32nd ask_price;                       // 8 bytes
    uint64_t bid_size;                        // 8 bytes
    uint64_t ask_size;                        // 8 bytes
    hft::HFTTimer::timestamp_t decision_time_ns; // 8 bytes
    uint64_t decision_latency_ns;             // 8 bytes
    uint8_t _pad1[8];                         // 8 bytes padding
    // Total: 64 bytes exactly
    
    TradingDecision() noexcept : action(Action::NO_ACTION), instrument(TreasuryType::Note_10Y) {
        std::memset(_pad0, 0, sizeof(_pad0));
        std::memset(_pad1, 0, sizeof(_pad1));
    }
};

// Atomic trading state for consistent reads/writes
struct alignas(CACHE_LINE_SIZE) AtomicTradingState {
    // Position and risk state
    std::array<std::atomic<int64_t>, 6> positions;     // Position per instrument (signed)
    std::atomic<int64_t> daily_pnl;                    // Daily P&L in cents
    std::atomic<uint64_t> order_count_today;           // Orders placed today
    std::atomic<double> portfolio_dv01;                // Current portfolio DV01
    
    // Current quotes state
    std::array<std::atomic<Price32nd>, 6> current_bid_prices;
    std::array<std::atomic<Price32nd>, 6> current_ask_prices;
    std::array<std::atomic<uint64_t>, 6> current_bid_sizes;
    std::array<std::atomic<uint64_t>, 6> current_ask_sizes;
    
    // State versioning for consistency
    std::atomic<uint64_t> state_version;
    
    AtomicTradingState() noexcept {
        // Initialize all atomic variables
        for (size_t i = 0; i < 6; ++i) {
            positions[i].store(0, std::memory_order_relaxed);
            current_bid_prices[i].store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
            current_ask_prices[i].store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
            current_bid_sizes[i].store(0, std::memory_order_relaxed);
            current_ask_sizes[i].store(0, std::memory_order_relaxed);
        }
        daily_pnl.store(0, std::memory_order_relaxed);
        order_count_today.store(0, std::memory_order_relaxed);
        portfolio_dv01.store(0.0, std::memory_order_relaxed);
        state_version.store(1, std::memory_order_relaxed);
    }
};

// Yield curve context for background processing
struct alignas(CACHE_LINE_SIZE) YieldCurveContext {
    std::array<double, 6> fair_yields;          // Fair yields per instrument
    std::array<double, 6> yield_volatilities;   // Recent yield volatilities
    std::array<Price32nd, 6> fair_prices;       // Fair prices from curve
    hft::HFTTimer::timestamp_t last_update_ns;  // When curve was last updated
    std::atomic<bool> curve_valid;              // Whether curve data is valid
    uint8_t _pad[47];                          // Padding to cache line
    
    YieldCurveContext() noexcept : last_update_ns(0) {
        fair_yields.fill(0.0);
        yield_volatilities.fill(0.0);
        for (auto& price : fair_prices) {
            price = Price32nd::from_decimal(0.0);
        }
        curve_valid.store(false, std::memory_order_relaxed);
        std::memset(_pad, 0, sizeof(_pad));
    }
};

// Main integrated market making strategy class
class IntegratedMarketMakingStrategy {
private:
    // Core infrastructure references
    ObjectPool<TreasuryOrder, 1000>& order_pool_;
    ObjectPool<MarketUpdate, 1000>& update_pool_;
    TreasuryOrderBook& order_book_;
    
    // Strategy state (cache-aligned)
    alignas(CACHE_LINE_SIZE) AtomicTradingState trading_state_;
    alignas(CACHE_LINE_SIZE) YieldCurveContext yield_curve_context_;
    
    // Performance tracking
    hft::HFTTimer timer_;
    mutable std::atomic<uint64_t> decision_count_;
    mutable std::atomic<uint64_t> total_decision_latency_ns_;
    
    // Strategy parameters (configurable)
    double base_spread_bps_;
    double inventory_penalty_bps_;
    double max_position_size_;
    
public:
    // Constructor with required infrastructure references
    IntegratedMarketMakingStrategy(
        ObjectPool<TreasuryOrder, 1000>& order_pool,
        ObjectPool<MarketUpdate, 1000>& update_pool,
        TreasuryOrderBook& order_book) noexcept
        : order_pool_(order_pool)
        , update_pool_(update_pool)
        , order_book_(order_book)
        , timer_()
        , decision_count_(0)
        , total_decision_latency_ns_(0)
        , base_spread_bps_(DEFAULT_SPREAD_BPS)
        , inventory_penalty_bps_(INVENTORY_PENALTY_BPS)
        , max_position_size_(MAX_POSITION_SIZE) {
    }
    
    // Delete copy/move to ensure single instance
    IntegratedMarketMakingStrategy(const IntegratedMarketMakingStrategy&) = delete;
    IntegratedMarketMakingStrategy& operator=(const IntegratedMarketMakingStrategy&) = delete;
    IntegratedMarketMakingStrategy(IntegratedMarketMakingStrategy&&) = delete;
    IntegratedMarketMakingStrategy& operator=(IntegratedMarketMakingStrategy&&) = delete;
    
    // Main strategy decision function - unified approach with adaptive complexity
    TradingDecision make_integrated_decision(const MarketUpdate& market_update) noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        auto remaining_budget_ns = STRATEGY_DECISION_TARGET_NS;
        
        TradingDecision decision;
        decision.instrument = market_update.instrument;
        decision.decision_time_ns = start_time;
        
        const auto instrument_index = static_cast<size_t>(market_update.instrument);
        
        // Declare all variables first to avoid goto bypass issues
        double bid_price_double = 0.0;
        double ask_price_double = 0.0;
        Price32nd mid_price;
        double inventory_adjustment = 0.0;
        uint64_t adjusted_quote_size = 0;
        double effective_spread_bps = 0.0;
        double spread_price = 0.0;
        uint64_t phase1_elapsed = 0;
        uint64_t phase3_start = 0;
        double dv01_exposure = 0.0;
        double current_portfolio_dv01 = 0.0;
        
        // Phase 1: Essential Analysis (400ns budget) - ALWAYS EXECUTE
        const auto phase1_start = timer_.get_timestamp_ns();
        
        // Fast position and risk lookup (50ns target)
        const auto current_position = trading_state_.positions[instrument_index].load(std::memory_order_relaxed);
        const auto daily_pnl = trading_state_.daily_pnl.load(std::memory_order_relaxed);
        const auto order_count = trading_state_.order_count_today.load(std::memory_order_relaxed);
        
        // Hard limit checks (50ns target)
        if (std::abs(current_position) >= max_position_size_ || 
            daily_pnl <= -MAX_DAILY_LOSS ||
            order_count >= MAX_ORDER_FREQUENCY) {
            decision.action = TradingDecision::Action::CANCEL_QUOTES;
            goto finalize_decision;
        }
        
        // Basic spread calculation (100ns target)
        mid_price = Price32nd::from_decimal(
            (market_update.best_bid.to_decimal() + market_update.best_ask.to_decimal()) / 2.0
        );
        
        inventory_adjustment = (static_cast<double>(current_position) / 10000000.0) * inventory_penalty_bps_;
        effective_spread_bps = base_spread_bps_ + std::abs(inventory_adjustment);
        spread_price = mid_price.to_decimal() * (effective_spread_bps / 10000.0);
        
        // Quote size determination (100ns target)
        {
            const uint64_t base_quote_size = 1000000; // $1M base size
            const double position_factor = 1.0 - (std::abs(current_position) / max_position_size_);
            adjusted_quote_size = static_cast<uint64_t>(base_quote_size * position_factor);
            
            if (adjusted_quote_size < 100000) { // Minimum $100k
                decision.action = TradingDecision::Action::CANCEL_QUOTES;
                goto finalize_decision;
            }
        }
        
        // Initial quote prices (treasury 32nds aligned)
        bid_price_double = mid_price.to_decimal() - spread_price - inventory_adjustment;
        ask_price_double = mid_price.to_decimal() + spread_price + inventory_adjustment;
        
        // Ensure 32nds alignment
        bid_price_double = std::floor(bid_price_double * TREASURY_32NDS_DENOMINATOR) / TREASURY_32NDS_DENOMINATOR;
        ask_price_double = std::ceil(ask_price_double * TREASURY_32NDS_DENOMINATOR) / TREASURY_32NDS_DENOMINATOR;
        
        decision.bid_price = Price32nd::from_decimal(bid_price_double);
        decision.ask_price = Price32nd::from_decimal(ask_price_double);
        decision.bid_size = adjusted_quote_size;
        decision.ask_size = adjusted_quote_size;
        
        phase1_elapsed = timer_.get_timestamp_ns() - phase1_start;
        remaining_budget_ns -= phase1_elapsed;
        
        // Phase 2: Enhanced Analysis (600ns budget) - IF BUDGET ALLOWS
        if (remaining_budget_ns > ENHANCED_PHASE_BUDGET_NS) {
            const auto phase2_start = timer_.get_timestamp_ns();
            
            // Order book imbalance analysis (150ns target)
            const double total_size = market_update.bid_size + market_update.ask_size;
            if (total_size > 0) {
                const double bid_imbalance = static_cast<double>(market_update.bid_size) / total_size;
                const double imbalance_adjustment = (bid_imbalance - 0.5) * 0.25; // Max 0.125bp adjustment
                
                bid_price_double += imbalance_adjustment;
                ask_price_double += imbalance_adjustment;
            }
            
            // Trade flow momentum detection (150ns target)
            if (market_update.last_trade_size > 0) {
                const double trade_momentum = (market_update.last_trade_price.to_decimal() - mid_price.to_decimal()) * 0.1;
                bid_price_double += trade_momentum;
                ask_price_double += trade_momentum;
            }
            
            // Yield curve dislocation check (150ns target)
            if (yield_curve_context_.curve_valid.load(std::memory_order_relaxed)) {
                const auto fair_price = yield_curve_context_.fair_prices[instrument_index];
                const double dislocation = mid_price.to_decimal() - fair_price.to_decimal();
                const double curve_adjustment = dislocation * 0.2; // 20% mean reversion
                
                bid_price_double -= curve_adjustment;
                ask_price_double -= curve_adjustment;
            }
            
            // Dynamic inventory adjustment (150ns target)
            const double time_to_close = 8.0; // Hours until close (simplified)
            const double decay_factor = std::max(0.1, time_to_close / 8.0);
            const double enhanced_inventory_penalty = inventory_adjustment / decay_factor;
            
            bid_price_double -= enhanced_inventory_penalty;
            ask_price_double += enhanced_inventory_penalty;
            
            const auto phase2_elapsed = timer_.get_timestamp_ns() - phase2_start;
            remaining_budget_ns -= phase2_elapsed;
        }
        
        // Phase 3: Quote Generation and Validation (200ns budget)
        phase3_start = timer_.get_timestamp_ns();
        
        // 32nds price normalization (50ns target)
        bid_price_double = std::floor(bid_price_double * TREASURY_32NDS_DENOMINATOR) / TREASURY_32NDS_DENOMINATOR;
        ask_price_double = std::ceil(ask_price_double * TREASURY_32NDS_DENOMINATOR) / TREASURY_32NDS_DENOMINATOR;
        
        decision.bid_price = Price32nd::from_decimal(bid_price_double);
        decision.ask_price = Price32nd::from_decimal(ask_price_double);
        
        // Risk-adjusted sizing (50ns target)
        dv01_exposure = DV01_PER_MILLION[instrument_index] * (adjusted_quote_size / 1000000.0);
        current_portfolio_dv01 = trading_state_.portfolio_dv01.load(std::memory_order_relaxed);
        
        if (current_portfolio_dv01 + dv01_exposure > DV01_PORTFOLIO_LIMIT) {
            const double size_reduction = (DV01_PORTFOLIO_LIMIT - current_portfolio_dv01) / dv01_exposure;
            if (size_reduction < 0.1) {
                decision.action = TradingDecision::Action::CANCEL_QUOTES;
                goto finalize_decision;
            }
            decision.bid_size = static_cast<uint64_t>(decision.bid_size * size_reduction);
            decision.ask_size = static_cast<uint64_t>(decision.ask_size * size_reduction);
        }
        
        // Quote validation (50ns target)
        if (decision.bid_price.to_decimal() >= decision.ask_price.to_decimal() ||
            decision.bid_size < 100000 || decision.ask_size < 100000) {
            decision.action = TradingDecision::Action::CANCEL_QUOTES;
            goto finalize_decision;
        }
        
        // Check if quotes need updating
        {
            const auto current_bid = trading_state_.current_bid_prices[instrument_index].load(std::memory_order_relaxed);
            const auto current_ask = trading_state_.current_ask_prices[instrument_index].load(std::memory_order_relaxed);
            
            const double bid_change = std::abs(decision.bid_price.to_decimal() - current_bid.to_decimal());
            const double ask_change = std::abs(decision.ask_price.to_decimal() - current_ask.to_decimal());
            
            if (bid_change > 0.5/32.0 || ask_change > 0.5/32.0) { // More than 0.5/32nds change
                decision.action = TradingDecision::Action::UPDATE_QUOTES;
            } else {
                decision.action = TradingDecision::Action::NO_ACTION;
            }
        }
        
    finalize_decision:
        const auto end_time = timer_.get_timestamp_ns();
        decision.decision_latency_ns = end_time - start_time;
        
        // Update performance tracking
        decision_count_.fetch_add(1, std::memory_order_relaxed);
        total_decision_latency_ns_.fetch_add(decision.decision_latency_ns, std::memory_order_relaxed);
        
        return decision;
    }
    
    // Update position after trade execution
    void update_position(TreasuryType instrument, int64_t quantity, Price32nd execution_price) noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        
        // Update position
        trading_state_.positions[instrument_index].fetch_add(quantity, std::memory_order_relaxed);
        
        // Update portfolio DV01
        const double dv01_change = DV01_PER_MILLION[instrument_index] * (quantity / 1000000.0);
        
        // Atomic update of portfolio DV01 (simplified - in production would need lock-free double update)
        auto current_dv01 = trading_state_.portfolio_dv01.load(std::memory_order_relaxed);
        while (!trading_state_.portfolio_dv01.compare_exchange_weak(
            current_dv01, current_dv01 + dv01_change, std::memory_order_relaxed)) {
            // Retry on failure
        }
        
        // Update state version for consistency tracking
        trading_state_.state_version.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Update background yield curve context (called from separate thread)
    void update_yield_curve_context(const std::array<double, 6>& yields,
                                  const std::array<double, 6>& volatilities) noexcept {
        for (size_t i = 0; i < 6; ++i) {
            yield_curve_context_.fair_yields[i] = yields[i];
            yield_curve_context_.yield_volatilities[i] = volatilities[i];
            
            // Convert yield to price (simplified treasury pricing)
            const double years_to_maturity = get_years_to_maturity(static_cast<TreasuryType>(i));
            const double price = 100.0 / std::pow(1.0 + yields[i]/100.0, years_to_maturity);
            yield_curve_context_.fair_prices[i] = Price32nd::from_decimal(price);
        }
        
        yield_curve_context_.last_update_ns = timer_.get_timestamp_ns();
        yield_curve_context_.curve_valid.store(true, std::memory_order_relaxed);
    }
    
    // Performance statistics
    struct PerformanceStats {
        uint64_t total_decisions;
        double average_latency_ns;
        double latency_percentile_95_ns;
        uint64_t decisions_per_second;
    };
    
    PerformanceStats get_performance_stats() const noexcept {
        const auto total_decisions = decision_count_.load(std::memory_order_relaxed);
        const auto total_latency = total_decision_latency_ns_.load(std::memory_order_relaxed);
        
        PerformanceStats stats{};
        stats.total_decisions = total_decisions;
        stats.average_latency_ns = total_decisions > 0 ? 
            static_cast<double>(total_latency) / total_decisions : 0.0;
        stats.latency_percentile_95_ns = stats.average_latency_ns * 1.2; // Approximation
        stats.decisions_per_second = 0; // Would need time tracking for actual calculation
        
        return stats;
    }
    
    // Get current trading state (for monitoring/debugging)
    const AtomicTradingState& get_current_state() const noexcept {
        return trading_state_; // Return reference to avoid copy of atomic state
    }
    
private:
    // Helper function for treasury maturity calculation
    static constexpr double get_years_to_maturity(TreasuryType type) noexcept {
        switch (type) {
            case TreasuryType::Bill_3M: return 0.25;
            case TreasuryType::Bill_6M: return 0.5;
            case TreasuryType::Note_2Y: return 2.0;
            case TreasuryType::Note_5Y: return 5.0;
            case TreasuryType::Note_10Y: return 10.0;
            case TreasuryType::Bond_30Y: return 30.0;
            default: return 10.0; // Default to 10Y
        }
    }
};

} // namespace strategy
} // namespace hft