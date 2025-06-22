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

namespace hft {
namespace strategy {

using namespace hft::market_data;

// Use common cache line size to avoid redefinition
namespace {
    constexpr size_t CACHE_LINE_SIZE = 64;
}

// Market data processing timing budgets
constexpr uint64_t LEVEL1_PROCESSING_BUDGET_NS = 200;    // Level 1 market data
constexpr uint64_t SIGNAL_EXTRACTION_BUDGET_NS = 150;    // Signal extraction
constexpr uint64_t CURVE_LOOKUP_BUDGET_NS = 50;          // Yield curve lookup

// Market data parameters
constexpr size_t TRADE_FLOW_HISTORY_SIZE = 32;           // Recent trades for momentum
constexpr size_t ORDER_BOOK_IMBALANCE_WINDOW = 16;       // Imbalance calculation window
constexpr double IMBALANCE_THRESHOLD = 0.7;              // 70% imbalance threshold
constexpr double MOMENTUM_DECAY_FACTOR = 0.95;           // Trade momentum decay
constexpr uint64_t STALE_DATA_THRESHOLD_NS = 100000000;  // 100ms stale threshold

// Yield curve background update frequency
constexpr uint64_t YIELD_CURVE_UPDATE_INTERVAL_NS = 500000000; // 500ms background updates
constexpr double CURVE_DISLOCATION_THRESHOLD_BPS = 0.5;  // 0.5bp dislocation threshold
constexpr size_t YIELD_CURVE_POINTS = 6;                 // Number of curve points

// Market microstructure signals
enum class MarketSignal : uint8_t {
    NEUTRAL = 0,
    BUY_PRESSURE = 1,
    SELL_PRESSURE = 2,
    IMBALANCED_BID = 3,
    IMBALANCED_ASK = 4,
    MOMENTUM_UP = 5,
    MOMENTUM_DOWN = 6,
    CURVE_RICH = 7,
    CURVE_CHEAP = 8
};

// Level 1 market data snapshot (cache-aligned)
struct alignas(CACHE_LINE_SIZE) Level1Snapshot {
    TreasuryType instrument;                           // 1 byte
    uint8_t _pad0[7];                                 // 7 bytes padding
    
    // Top-of-book data
    Price32nd best_bid;                               // 8 bytes
    Price32nd best_ask;                               // 8 bytes
    uint64_t bid_size;                                // 8 bytes
    uint64_t ask_size;                                // 8 bytes
    
    // Recent trade information
    Price32nd last_trade_price;                       // 8 bytes
    uint64_t last_trade_size;                         // 8 bytes
    uint32_t trades_in_last_second;                   // 4 bytes
    uint32_t volume_in_last_second;                   // 4 bytes
    
    // Timing
    hft::HFTTimer::timestamp_t snapshot_time;         // 8 bytes
    uint8_t _pad1[8];                                // 8 bytes padding
    // Total: 64 bytes exactly
    
    Level1Snapshot() noexcept : instrument(TreasuryType::Note_10Y) {
        std::memset(_pad0, 0, sizeof(_pad0));
        std::memset(_pad1, 0, sizeof(_pad1));
        best_bid = Price32nd::from_decimal(0.0);
        best_ask = Price32nd::from_decimal(0.0);
        bid_size = 0;
        ask_size = 0;
        last_trade_price = Price32nd::from_decimal(0.0);
        last_trade_size = 0;
        trades_in_last_second = 0;
        volume_in_last_second = 0;
        snapshot_time = 0;
    }
};

// Trade flow record for momentum analysis
struct TradeFlowRecord {
    Price32nd trade_price;                            // 8 bytes
    uint64_t trade_size;                              // 8 bytes
    hft::HFTTimer::timestamp_t trade_time;            // 8 bytes
    bool is_buyer_initiated;                          // 1 byte (aggressor side)
    uint8_t _pad[7];                                 // 7 bytes padding
    // Total: 32 bytes (cache-friendly)
    
    TradeFlowRecord() noexcept 
        : trade_price(Price32nd::from_decimal(0.0)), trade_size(0), trade_time(0), is_buyer_initiated(false) {
        std::memset(_pad, 0, sizeof(_pad));
    }
};

// Order book imbalance snapshot
struct OrderBookImbalance {
    double bid_pressure;                              // Normalized bid pressure (0-1)
    double ask_pressure;                              // Normalized ask pressure (0-1)
    double imbalance_ratio;                           // Bid/(Bid+Ask) ratio
    double size_weighted_mid;                         // Size-weighted midpoint
    hft::HFTTimer::timestamp_t calculation_time;
    
    OrderBookImbalance() noexcept 
        : bid_pressure(0.5), ask_pressure(0.5), imbalance_ratio(0.5), 
          size_weighted_mid(0.0), calculation_time(0) {}
};

// Background yield curve state (updated every 500ms)
struct alignas(CACHE_LINE_SIZE) YieldCurveState {
    // Fair value prices from yield curve fitting
    std::array<Price32nd, 6> fair_value_prices;      // 48 bytes
    std::array<double, 6> fair_yields;                // 48 bytes (not cache-aligned but close)
    
    // Curve fitting quality metrics
    double curve_fit_r_squared;                       // 8 bytes
    double max_residual_bps;                          // 8 bytes
    hft::HFTTimer::timestamp_t last_curve_update;     // 8 bytes
    std::atomic<bool> curve_valid;                    // 1 byte
    uint8_t _pad[47];                                // Padding to next cache line
    
    YieldCurveState() noexcept 
        : curve_fit_r_squared(0.0), max_residual_bps(0.0), last_curve_update(0) {
        for (auto& price : fair_value_prices) {
            price = Price32nd::from_decimal(0.0);
        }
        fair_yields.fill(0.0);
        curve_valid.store(false, std::memory_order_relaxed);
        std::memset(_pad, 0, sizeof(_pad));
    }
};

// Market signal analysis result
struct MarketSignalAnalysis {
    MarketSignal primary_signal;                      // Strongest signal detected
    MarketSignal secondary_signal;                    // Secondary signal
    double signal_strength;                           // Signal confidence (0-1)
    double price_adjustment_bps;                      // Suggested price adjustment
    double size_adjustment_factor;                    // Suggested size adjustment
    uint64_t analysis_latency_ns;                     // Time taken for analysis
    
    MarketSignalAnalysis() noexcept 
        : primary_signal(MarketSignal::NEUTRAL), secondary_signal(MarketSignal::NEUTRAL),
          signal_strength(0.0), price_adjustment_bps(0.0), size_adjustment_factor(1.0),
          analysis_latency_ns(0) {}
};

// Main market data processor class
class MarketDataProcessor {
private:
    // Market data state per instrument (cache-aligned)
    alignas(CACHE_LINE_SIZE) std::array<Level1Snapshot, 6> level1_snapshots_;
    
    // Trade flow history per instrument
    std::array<std::array<TradeFlowRecord, TRADE_FLOW_HISTORY_SIZE>, 6> trade_flow_history_;
    std::array<std::atomic<size_t>, 6> trade_flow_indices_;
    
    // Order book imbalance tracking
    std::array<std::atomic<OrderBookImbalance>, 6> current_imbalances_;
    
    // Background yield curve state
    alignas(CACHE_LINE_SIZE) YieldCurveState yield_curve_state_;
    
    // Performance tracking
    hft::HFTTimer timer_;
    mutable std::atomic<uint64_t> level1_updates_processed_;
    mutable std::atomic<uint64_t> signal_analyses_performed_;
    mutable std::atomic<uint64_t> total_processing_latency_ns_;
    mutable std::atomic<uint64_t> curve_lookups_performed_;
    
public:
    MarketDataProcessor() noexcept
        : timer_()
        , level1_updates_processed_(0)
        , signal_analyses_performed_(0)
        , total_processing_latency_ns_(0)
        , curve_lookups_performed_(0) {
        
        // Initialize snapshots for each instrument
        for (size_t i = 0; i < 6; ++i) {
            level1_snapshots_[i] = Level1Snapshot{};
            level1_snapshots_[i].instrument = static_cast<TreasuryType>(i);
            trade_flow_indices_[i].store(0, std::memory_order_relaxed);
            
            // Initialize trade flow history
            for (auto& record : trade_flow_history_[i]) {
                record = TradeFlowRecord{};
            }
        }
    }
    
    // Delete copy/move constructors
    MarketDataProcessor(const MarketDataProcessor&) = delete;
    MarketDataProcessor& operator=(const MarketDataProcessor&) = delete;
    MarketDataProcessor(MarketDataProcessor&&) = delete;
    MarketDataProcessor& operator=(MarketDataProcessor&&) = delete;
    
    // Update Level 1 market data (target: 200ns)
    bool update_level1_data(const Level1Snapshot& new_snapshot) noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        const auto instrument_index = static_cast<size_t>(new_snapshot.instrument);
        
        // Validate data freshness (20ns)
        const auto current_time = timer_.get_timestamp_ns();
        if (current_time - new_snapshot.snapshot_time > STALE_DATA_THRESHOLD_NS) {
            return false; // Data too stale
        }
        
        // Update Level 1 snapshot (50ns)
        level1_snapshots_[instrument_index] = new_snapshot;
        
        // Calculate order book imbalance (80ns)
        OrderBookImbalance imbalance;
        const auto total_size = new_snapshot.bid_size + new_snapshot.ask_size;
        
        if (total_size > 0) {
            imbalance.bid_pressure = static_cast<double>(new_snapshot.bid_size) / total_size;
            imbalance.ask_pressure = static_cast<double>(new_snapshot.ask_size) / total_size;
            imbalance.imbalance_ratio = imbalance.bid_pressure;
            
            // Size-weighted midpoint
            const double bid_weight = imbalance.bid_pressure;
            const double ask_weight = imbalance.ask_pressure;
            imbalance.size_weighted_mid = 
                new_snapshot.best_bid.to_decimal() * ask_weight + 
                new_snapshot.best_ask.to_decimal() * bid_weight;
        } else {
            imbalance.bid_pressure = 0.5;
            imbalance.ask_pressure = 0.5;
            imbalance.imbalance_ratio = 0.5;
            imbalance.size_weighted_mid = (new_snapshot.best_bid.to_decimal() + 
                                          new_snapshot.best_ask.to_decimal()) / 2.0;
        }
        
        imbalance.calculation_time = current_time;
        
        // Store imbalance (simplified atomic store)
        current_imbalances_[instrument_index].store(imbalance, std::memory_order_relaxed);
        
        // Update performance tracking (50ns)
        const auto end_time = timer_.get_timestamp_ns();
        level1_updates_processed_.fetch_add(1, std::memory_order_relaxed);
        total_processing_latency_ns_.fetch_add(end_time - start_time, std::memory_order_relaxed);
        
        return true;
    }
    
    // Add trade to flow history for momentum analysis (target: 100ns)
    void add_trade_flow(TreasuryType instrument, const TradeFlowRecord& trade) noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        
        // Get next index in circular buffer (20ns)
        const auto current_index = trade_flow_indices_[instrument_index].load(std::memory_order_relaxed);
        const auto next_index = (current_index + 1) % TRADE_FLOW_HISTORY_SIZE;
        
        // Store trade record (30ns)
        trade_flow_history_[instrument_index][next_index] = trade;
        
        // Update index (20ns)
        trade_flow_indices_[instrument_index].store(next_index, std::memory_order_relaxed);
        
        // Update Level 1 snapshot with latest trade info (30ns)
        auto& snapshot = level1_snapshots_[instrument_index];
        snapshot.last_trade_price = trade.trade_price;
        snapshot.last_trade_size = trade.trade_size;
        
        // Update trade count and volume (simplified - would need time window tracking)
        snapshot.trades_in_last_second++;
        snapshot.volume_in_last_second += trade.trade_size;
    }
    
    // Perform comprehensive market signal analysis (target: 150ns)
    MarketSignalAnalysis analyze_market_signals(TreasuryType instrument) noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        const auto instrument_index = static_cast<size_t>(instrument);
        MarketSignalAnalysis analysis;
        
        // Order book imbalance analysis (40ns)
        const auto imbalance = current_imbalances_[instrument_index].load(std::memory_order_relaxed);
        
        if (imbalance.imbalance_ratio > IMBALANCE_THRESHOLD) {
            analysis.primary_signal = MarketSignal::IMBALANCED_BID;
            analysis.signal_strength = (imbalance.imbalance_ratio - 0.5) * 2.0; // Scale to 0-1
            analysis.price_adjustment_bps = analysis.signal_strength * 0.25; // Max 0.25bp adjustment
        } else if (imbalance.imbalance_ratio < (1.0 - IMBALANCE_THRESHOLD)) {
            analysis.primary_signal = MarketSignal::IMBALANCED_ASK;
            analysis.signal_strength = (0.5 - imbalance.imbalance_ratio) * 2.0;
            analysis.price_adjustment_bps = -analysis.signal_strength * 0.25;
        }
        
        // Trade flow momentum analysis (50ns)
        const auto& trade_history = trade_flow_history_[instrument_index];
        const auto current_index = trade_flow_indices_[instrument_index].load(std::memory_order_relaxed);
        
        double momentum_score = 0.0;
        double decay_factor = 1.0;
        size_t valid_trades = 0;
        
        // Analyze recent trades (up to 16 most recent)
        for (size_t i = 0; i < std::min(TRADE_FLOW_HISTORY_SIZE, static_cast<size_t>(16)); ++i) {
            const size_t idx = (current_index + TRADE_FLOW_HISTORY_SIZE - i) % TRADE_FLOW_HISTORY_SIZE;
            const auto& trade = trade_history[idx];
            
            if (trade.trade_time == 0) break; // No more valid trades
            
            // Weight by recency and size
            const double weight = decay_factor * std::min(1.0, trade.trade_size / 1000000.0);
            momentum_score += trade.is_buyer_initiated ? weight : -weight;
            
            decay_factor *= MOMENTUM_DECAY_FACTOR;
            valid_trades++;
        }
        
        if (valid_trades > 0) {
            momentum_score /= valid_trades; // Normalize
            
            if (momentum_score > 0.3) {
                analysis.secondary_signal = MarketSignal::MOMENTUM_UP;
                analysis.price_adjustment_bps += momentum_score * 0.2; // Max 0.2bp from momentum
            } else if (momentum_score < -0.3) {
                analysis.secondary_signal = MarketSignal::MOMENTUM_DOWN;
                analysis.price_adjustment_bps += momentum_score * 0.2;
            }
        }
        
        // Yield curve dislocation analysis (60ns)
        if (yield_curve_state_.curve_valid.load(std::memory_order_relaxed)) {
            const auto& level1 = level1_snapshots_[instrument_index];
            const auto current_mid = (level1.best_bid.to_decimal() + level1.best_ask.to_decimal()) / 2.0;
            const auto fair_value = yield_curve_state_.fair_value_prices[instrument_index].to_decimal();
            
            const double dislocation_bps = (current_mid - fair_value) * 10000.0; // Convert to basis points
            
            if (std::abs(dislocation_bps) > CURVE_DISLOCATION_THRESHOLD_BPS) {
                if (analysis.primary_signal == MarketSignal::NEUTRAL) {
                    analysis.primary_signal = (dislocation_bps > 0) ? MarketSignal::CURVE_RICH : MarketSignal::CURVE_CHEAP;
                    analysis.signal_strength = std::min(1.0, std::abs(dislocation_bps) / 2.0); // 2bp = full strength
                }
                
                // Mean reversion adjustment
                analysis.price_adjustment_bps -= dislocation_bps * 0.1; // 10% mean reversion
            }
        }
        
        // Size adjustment based on signal strength
        analysis.size_adjustment_factor = 0.8 + (analysis.signal_strength * 0.4); // 0.8 to 1.2 range
        
        // Finalize analysis
        const auto end_time = timer_.get_timestamp_ns();
        analysis.analysis_latency_ns = end_time - start_time;
        
        signal_analyses_performed_.fetch_add(1, std::memory_order_relaxed);
        
        return analysis;
    }
    
    // Fast yield curve lookup (target: 50ns)
    Price32nd get_fair_value_price(TreasuryType instrument) const noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        if (!yield_curve_state_.curve_valid.load(std::memory_order_relaxed)) {
            return Price32nd::from_decimal(0.0); // No valid curve
        }
        
        const auto instrument_index = static_cast<size_t>(instrument);
        const auto fair_price = yield_curve_state_.fair_value_prices[instrument_index];
        
        curve_lookups_performed_.fetch_add(1, std::memory_order_relaxed);
        
        return fair_price;
    }
    
    // Update background yield curve (called every 500ms from background thread)
    void update_yield_curve(const std::array<double, 6>& new_yields,
                          const std::array<Price32nd, 6>& new_fair_prices,
                          double r_squared, double max_residual) noexcept {
        // Update yields and prices
        yield_curve_state_.fair_yields = new_yields;
        yield_curve_state_.fair_value_prices = new_fair_prices;
        
        // Update quality metrics
        yield_curve_state_.curve_fit_r_squared = r_squared;
        yield_curve_state_.max_residual_bps = max_residual;
        yield_curve_state_.last_curve_update = timer_.get_timestamp_ns();
        
        // Mark curve as valid if quality is acceptable
        const bool curve_quality_good = (r_squared > 0.95 && max_residual < 2.0); // R² > 95%, residual < 2bp
        yield_curve_state_.curve_valid.store(curve_quality_good, std::memory_order_relaxed);
    }
    
    // Get current Level 1 data
    Level1Snapshot get_level1_snapshot(TreasuryType instrument) const noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        return level1_snapshots_[instrument_index];
    }
    
    // Get current order book imbalance
    OrderBookImbalance get_order_book_imbalance(TreasuryType instrument) const noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        return current_imbalances_[instrument_index].load(std::memory_order_relaxed);
    }
    
    // Get yield curve state
    YieldCurveState get_yield_curve_state() const noexcept {
        return yield_curve_state_;
    }
    
    // Performance statistics
    struct MarketDataProcessorStats {
        uint64_t level1_updates_processed;
        uint64_t signal_analyses_performed;
        uint64_t curve_lookups_performed;
        double average_processing_latency_ns;
        bool yield_curve_valid;
        double curve_quality_r_squared;
        hft::HFTTimer::timestamp_t last_curve_update;
        std::array<uint32_t, 6> trades_per_second_by_instrument;
    };
    
    MarketDataProcessorStats get_performance_stats() const noexcept {
        const auto level1_updates = level1_updates_processed_.load(std::memory_order_relaxed);
        const auto signal_analyses = signal_analyses_performed_.load(std::memory_order_relaxed);
        const auto curve_lookups = curve_lookups_performed_.load(std::memory_order_relaxed);
        const auto total_latency = total_processing_latency_ns_.load(std::memory_order_relaxed);
        
        MarketDataProcessorStats stats{};
        stats.level1_updates_processed = level1_updates;
        stats.signal_analyses_performed = signal_analyses;
        stats.curve_lookups_performed = curve_lookups;
        stats.average_processing_latency_ns = level1_updates > 0 ? 
            static_cast<double>(total_latency) / level1_updates : 0.0;
        stats.yield_curve_valid = yield_curve_state_.curve_valid.load(std::memory_order_relaxed);
        stats.curve_quality_r_squared = yield_curve_state_.curve_fit_r_squared;
        stats.last_curve_update = yield_curve_state_.last_curve_update;
        
        for (size_t i = 0; i < 6; ++i) {
            stats.trades_per_second_by_instrument[i] = level1_snapshots_[i].trades_in_last_second;
        }
        
        return stats;
    }
};

} // namespace strategy
} // namespace hft