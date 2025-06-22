#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>
#include <utility>
#include <cassert>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/market_data/treasury_instruments.hpp"

namespace hft {
namespace strategy {

using namespace hft::market_data;
using namespace hft::trading;

// ARM64 cache line size
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Simple market making strategy for treasury markets
 * 
 * This implementation provides <1μs decision latency through:
 * - Cache-aligned data structures optimized for ARM64
 * - Zero-allocation operations using object pools
 * - Linear control flow without complex branching
 * - Treasury market compliance with 32nds pricing
 * - Position-based spread adjustment
 * - Simple risk management
 * 
 * Design patterns:
 * - Single-header component following established patterns
 * - Object pool integration for zero allocation
 * - HFT timer integration for performance measurement
 * - Treasury pricing and yield calculations
 * 
 * Performance targets:
 * - Decision latency: <1μs (significant headroom available)
 * - Zero allocation in hot paths
 * - Cache-optimized data layout
 */
class alignas(CACHE_LINE_SIZE) SimpleMarketMaker {
public:
    // Trading decision structure - cache-aligned for performance
    struct alignas(CACHE_LINE_SIZE) TradingDecision {
        enum class Action : uint8_t { 
            NO_ACTION = 0, 
            UPDATE_QUOTES = 1, 
            CANCEL_QUOTES = 2 
        };
        
        Action action = Action::NO_ACTION;               // 1 byte
        uint8_t _pad0[3];                               // 3 bytes padding
        TreasuryType instrument;                        // 1 byte
        uint8_t _pad1[3];                               // 3 bytes padding
        Price32nd bid_price;                            // 8 bytes
        Price32nd ask_price;                            // 8 bytes
        uint64_t bid_size = 0;                          // 8 bytes
        uint64_t ask_size = 0;                          // 8 bytes
        uint64_t decision_latency_ns = 0;               // 8 bytes
        uint64_t decision_time_ns = 0;                  // 8 bytes
        uint8_t _pad2[8];                               // 8 bytes padding (total: 64)
        
        TradingDecision() noexcept : _pad0{}, _pad1{}, _pad2{} {}
    };
    static_assert(sizeof(TradingDecision) == CACHE_LINE_SIZE, "TradingDecision must be 64 bytes");

    // Market update structure for strategy input
    struct MarketUpdate {
        TreasuryType instrument;
        Price32nd best_bid;
        Price32nd best_ask;
        uint64_t bid_size;
        uint64_t ask_size;
        uint64_t update_time_ns;
        
        MarketUpdate() noexcept = default;
        MarketUpdate(TreasuryType inst, Price32nd bid, Price32nd ask, 
                    uint64_t bid_sz, uint64_t ask_sz) noexcept
            : instrument(inst), best_bid(bid), best_ask(ask), 
              bid_size(bid_sz), ask_size(ask_sz), 
              update_time_ns(hft::HFTTimer::get_timestamp_ns()) {}
    };

    /**
     * @brief Constructor with object pool and order book integration
     * @param order_pool Object pool for order allocation
     * @param order_book Order book reference for market data
     */
    explicit SimpleMarketMaker(
        hft::ObjectPool<TreasuryOrder, 4096>& order_pool,
        TreasuryOrderBook& order_book
    ) noexcept
        : order_pool_(order_pool), order_book_(order_book),
          positions_{}, decision_count_(0), total_decision_latency_ns_(0),
          portfolio_var_(0.0), last_risk_update_ns_(0), instrument_volatilities_{} {
        
        // Initialize positions to zero
        for (auto& pos : positions_) {
            pos.net_position = 0;
            pos.unrealized_pnl = 0.0;
            pos.last_update_time = 0;
            pos.average_price = 0.0;
            pos.daily_pnl = 0.0;
            pos.position_var = 0.0;
            pos.trade_count = 0;
            pos.risk_score = 0;
        }
        
        // Initialize volatilities to reasonable treasury defaults
        instrument_volatilities_[0] = 0.001; // 3M Bill
        instrument_volatilities_[1] = 0.002; // 6M Bill
        instrument_volatilities_[2] = 0.005; // 2Y Note
        instrument_volatilities_[3] = 0.008; // 5Y Note
        instrument_volatilities_[4] = 0.012; // 10Y Note
        instrument_volatilities_[5] = 0.015; // 30Y Bond
    }

    // No copy or move semantics for performance
    SimpleMarketMaker(const SimpleMarketMaker&) = delete;
    SimpleMarketMaker& operator=(const SimpleMarketMaker&) = delete;

    /**
     * @brief Main decision function - target <1μs latency
     * @param update Market data update
     * @return Trading decision with quotes or action
     */
    [[nodiscard]] TradingDecision make_decision(const MarketUpdate& update) noexcept;

    /**
     * @brief Update position after trade execution
     * @param instrument Treasury instrument type
     * @param size_change Position change (positive for buy, negative for sell)
     * @param fill_price Execution price
     */
    void update_position(TreasuryType instrument, int64_t size_change, Price32nd fill_price) noexcept;

    /**
     * @brief Check if proposed order size is within risk limits
     * @param instrument Treasury instrument type
     * @param order_size Proposed order size
     * @return true if within limits, false otherwise
     */
    [[nodiscard]] bool check_risk_limits(TreasuryType instrument, uint64_t order_size) const noexcept;

    /**
     * @brief Get average decision latency in nanoseconds
     * @return Average decision latency
     */
    [[nodiscard]] uint64_t get_average_decision_latency_ns() const noexcept {
        return decision_count_ > 0 ? total_decision_latency_ns_ / decision_count_ : 0;
    }

    /**
     * @brief Get total number of decisions made
     * @return Decision count
     */
    [[nodiscard]] uint64_t get_decision_count() const noexcept {
        return decision_count_;
    }

    /**
     * @brief Get current position for instrument
     * @param instrument Treasury instrument type
     * @return Current net position in dollars
     */
    [[nodiscard]] int64_t get_position(TreasuryType instrument) const noexcept {
        const auto index = static_cast<size_t>(instrument);
        return (index < positions_.size()) ? positions_[index].net_position : 0;
    }

    /**
     * @brief Get unrealized P&L for instrument
     * @param instrument Treasury instrument type  
     * @return Unrealized P&L in dollars
     */
    [[nodiscard]] double get_unrealized_pnl(TreasuryType instrument) const noexcept {
        const auto index = static_cast<size_t>(instrument);
        return (index < positions_.size()) ? positions_[index].unrealized_pnl : 0.0;
    }
    
    // Phase 2: Enhanced position and risk management methods
    
    /**
     * @brief Get daily P&L for instrument (realized + unrealized)
     * @param instrument Treasury instrument type
     * @return Daily P&L in dollars
     */
    [[nodiscard]] double get_daily_pnl(TreasuryType instrument) const noexcept {
        const auto index = static_cast<size_t>(instrument);
        return (index < positions_.size()) ? positions_[index].daily_pnl : 0.0;
    }
    
    /**
     * @brief Get portfolio-level VaR estimate
     * @return Portfolio VaR in dollars
     */
    [[nodiscard]] double get_portfolio_var() const noexcept {
        return portfolio_var_;
    }
    
    /**
     * @brief Get risk score for instrument (0-1000)
     * @param instrument Treasury instrument type
     * @return Risk score (higher = riskier)
     */
    [[nodiscard]] uint32_t get_risk_score(TreasuryType instrument) const noexcept {
        const auto index = static_cast<size_t>(instrument);
        return (index < positions_.size()) ? positions_[index].risk_score : 1000;
    }
    
    /**
     * @brief Update market prices for mark-to-market calculation
     * @param instrument Treasury instrument type
     * @param current_price Current market price
     */
    void update_market_price(TreasuryType instrument, Price32nd current_price) noexcept;
    
    /**
     * @brief Recalculate portfolio-level risk metrics
     */
    void update_portfolio_risk() noexcept;

private:
    // Configuration constants
    static constexpr double BASE_SPREAD_BPS = 0.5;           // 0.5 basis points base spread
    static constexpr double INVENTORY_PENALTY_BPS = 1.0;     // 1.0bp per $10M position
    static constexpr int64_t MAX_POSITION_SIZE = 50000000;   // $50M position limit
    static constexpr int64_t BASE_QUOTE_SIZE = 1000000;      // $1M base quote size
    static constexpr uint64_t TARGET_DECISION_NS = 1000;     // 1μs target latency
    static constexpr double MIN_SPREAD_BPS = 0.25;           // Minimum 0.25bp spread
    
    // Phase 2: Enhanced risk management constants
    static constexpr double MAX_PORTFOLIO_VAR = 5000000.0;   // $5M daily VaR limit
    static constexpr double CORRELATION_FACTOR = 0.7;        // Inter-instrument correlation
    static constexpr double VOLATILITY_THRESHOLD = 0.02;     // 2% volatility threshold
    static constexpr uint64_t POSITION_UPDATE_INTERVAL_NS = 1000000; // 1ms update interval
    static constexpr double SKEW_PENALTY_BPS = 0.5;          // Skewing penalty in bps
    static constexpr double MAX_SKEW_RATIO = 3.0;            // Max skew ratio (3:1)

    // Enhanced position tracking structure
    struct alignas(CACHE_LINE_SIZE) Position {
        int64_t net_position = 0;                           // Net position in dollars
        double unrealized_pnl = 0.0;                       // Mark-to-market P&L
        uint64_t last_update_time = 0;                     // Last update timestamp
        
        // Phase 2: Enhanced position tracking
        double average_price = 0.0;                        // Volume-weighted average price
        double daily_pnl = 0.0;                            // Realized + unrealized daily P&L
        double position_var = 0.0;                         // Position-level VaR estimate
        int32_t trade_count = 0;                           // Number of trades today
        uint32_t risk_score = 0;                           // Computed risk score (0-1000)
        
        uint8_t _pad[8];                                    // Padding to cache line
        
        Position() noexcept : _pad{} {}
    };
    static_assert(sizeof(Position) == CACHE_LINE_SIZE, "Position must be 64 bytes");

    // Infrastructure references - cache-aligned
    alignas(CACHE_LINE_SIZE) hft::ObjectPool<TreasuryOrder, 4096>& order_pool_;
    alignas(CACHE_LINE_SIZE) TreasuryOrderBook& order_book_;
    alignas(CACHE_LINE_SIZE) hft::HFTTimer timer_;

    // Position tracking (6 treasury types) - cache-aligned
    alignas(CACHE_LINE_SIZE) std::array<Position, 6> positions_;

    // Performance tracking - cache-aligned
    alignas(CACHE_LINE_SIZE) uint64_t decision_count_;
    alignas(CACHE_LINE_SIZE) uint64_t total_decision_latency_ns_;
    
    // Phase 2: Enhanced risk tracking - cache-aligned
    alignas(CACHE_LINE_SIZE) double portfolio_var_;
    alignas(CACHE_LINE_SIZE) uint64_t last_risk_update_ns_;
    alignas(CACHE_LINE_SIZE) std::array<double, 6> instrument_volatilities_;

    // Core logic helper functions

    /**
     * @brief Calculate spread in basis points based on inventory
     * @param instrument Treasury instrument type
     * @return Spread in basis points
     */
    [[nodiscard]] double calculate_spread_bps(TreasuryType instrument) const noexcept;

    /**
     * @brief Calculate bid and ask quotes based on market data and spread
     * @param update Market data update
     * @param spread_bps Spread in basis points
     * @return Pair of bid and ask prices
     */
    [[nodiscard]] std::pair<Price32nd, Price32nd> calculate_quotes(
        const MarketUpdate& update, 
        double spread_bps
    ) const noexcept;

    /**
     * @brief Calculate quote size based on instrument and position
     * @param instrument Treasury instrument type
     * @return Quote size in dollars
     */
    [[nodiscard]] uint64_t calculate_quote_size(TreasuryType instrument) const noexcept;

    /**
     * @brief Validate market update for basic sanity
     * @param update Market data update
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool is_valid_market_update(const MarketUpdate& update) const noexcept {
        return update.best_bid.whole > 0 && 
               update.best_ask.whole > 0 && 
               update.bid_size > 0 && 
               update.ask_size > 0 &&
               price_less_equal(update.best_bid, update.best_ask);
    }
    
    // Phase 2: Enhanced risk management helpers
    
    /**
     * @brief Calculate position-specific risk score
     * @param instrument Treasury instrument type
     * @return Risk score (0-1000)
     */
    [[nodiscard]] uint32_t calculate_position_risk_score(TreasuryType instrument) const noexcept;
    
    /**
     * @brief Calculate portfolio-level VaR using simplified model
     * @return Portfolio VaR estimate in dollars
     */
    [[nodiscard]] double calculate_portfolio_var() const noexcept;
    
    /**
     * @brief Calculate position-aware quote skewing
     * @param instrument Treasury instrument type
     * @param base_bid Base bid price
     * @param base_ask Base ask price
     * @return Skewed bid and ask prices
     */
    [[nodiscard]] std::pair<Price32nd, Price32nd> apply_position_skewing(
        TreasuryType instrument,
        Price32nd base_bid,
        Price32nd base_ask
    ) const noexcept;
    
    /**
     * @brief Enhanced risk check including portfolio constraints
     * @param instrument Treasury instrument type
     * @param order_size Proposed order size
     * @return true if within all risk limits
     */
    [[nodiscard]] bool check_enhanced_risk_limits(TreasuryType instrument, uint64_t order_size) const noexcept;

    // Price comparison helpers for 32nd fractional pricing
    [[nodiscard]] static bool price_less_equal(const Price32nd& a, const Price32nd& b) noexcept {
        if (a.whole != b.whole) return a.whole < b.whole;
        if (a.thirty_seconds != b.thirty_seconds) return a.thirty_seconds < b.thirty_seconds;
        return a.half_32nds <= b.half_32nds;
    }
};

// Implementation of inline methods

inline SimpleMarketMaker::TradingDecision SimpleMarketMaker::make_decision(const MarketUpdate& update) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    TradingDecision decision;
    decision.instrument = update.instrument;
    decision.decision_time_ns = start_time;
    decision.action = TradingDecision::Action::NO_ACTION;
    
    // Step 1: Basic validation (target: 50ns)
    if (is_valid_market_update(update)) {
        // Step 2: Enhanced risk checks (target: 150ns)
        if (check_enhanced_risk_limits(update.instrument, BASE_QUOTE_SIZE)) {
            // Step 3: Calculate spread based on inventory (target: 200ns)
            const double spread_bps = calculate_spread_bps(update.instrument);
            
            // Step 4: Generate base quotes (target: 300ns)
            const auto [base_bid, base_ask] = calculate_quotes(update, spread_bps);
            
            // Step 5: Apply position skewing (target: 100ns)
            const auto [bid_price, ask_price] = apply_position_skewing(update.instrument, base_bid, base_ask);
            
            // Step 6: Size calculation (target: 100ns)
            const uint64_t quote_size = calculate_quote_size(update.instrument);
            
            // Step 7: Final decision assembly (target: 50ns)
            decision.action = TradingDecision::Action::UPDATE_QUOTES;
            decision.bid_price = bid_price;
            decision.ask_price = ask_price;
            decision.bid_size = quote_size;
            decision.ask_size = quote_size;
        } else {
            decision.action = TradingDecision::Action::CANCEL_QUOTES;
        }
    }
    
    const auto end_time = timer_.get_timestamp_ns();
    decision.decision_latency_ns = end_time - start_time;
    
    // Update performance tracking
    ++decision_count_;
    total_decision_latency_ns_ += decision.decision_latency_ns;
    
    return decision;
}

inline double SimpleMarketMaker::calculate_spread_bps(TreasuryType instrument) const noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return BASE_SPREAD_BPS;
    }
    
    const auto& position = positions_[instrument_index];
    
    // Base spread
    double spread_bps = BASE_SPREAD_BPS;
    
    // Inventory penalty (linear model)
    const double position_millions = position.net_position / 1000000.0; // Convert to millions
    const double inventory_penalty = std::abs(position_millions) * INVENTORY_PENALTY_BPS;
    
    spread_bps += inventory_penalty;
    
    // Ensure minimum spread for treasury markets
    return std::max(spread_bps, MIN_SPREAD_BPS);
}

inline std::pair<Price32nd, Price32nd> SimpleMarketMaker::calculate_quotes(
    const MarketUpdate& update, 
    double spread_bps
) const noexcept {
    
    // Calculate mid-market price
    const double market_bid = update.best_bid.to_decimal();
    const double market_ask = update.best_ask.to_decimal();
    const double mid_price = (market_bid + market_ask) / 2.0;
    
    // Apply spread in basis points relative to mid price
    const double spread_decimal = spread_bps / 10000.0; // Convert bps to decimal
    const double half_spread = mid_price * spread_decimal / 2.0;
    
    // Generate quotes around mid-market (bid below mid, ask above mid)
    const double quote_bid_decimal = mid_price - half_spread;
    const double quote_ask_decimal = mid_price + half_spread;
    
    // Convert to 32nds (automatic treasury compliance)
    const Price32nd bid_price = Price32nd::from_decimal(quote_bid_decimal);
    const Price32nd ask_price = Price32nd::from_decimal(quote_ask_decimal);
    
    return {bid_price, ask_price};
}

inline uint64_t SimpleMarketMaker::calculate_quote_size(TreasuryType instrument) const noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return BASE_QUOTE_SIZE;
    }
    
    const auto& position = positions_[instrument_index];
    
    // Start with base size
    uint64_t quote_size = BASE_QUOTE_SIZE;
    
    // Reduce size if approaching position limits
    const int64_t abs_position = std::abs(position.net_position);
    if (abs_position > MAX_POSITION_SIZE / 2) {
        // Reduce quote size as we approach limits
        const double reduction_factor = 1.0 - (static_cast<double>(abs_position) / MAX_POSITION_SIZE);
        quote_size = static_cast<uint64_t>(quote_size * std::max(reduction_factor, 0.1)); // Min 10% of base
    }
    
    return quote_size;
}

inline bool SimpleMarketMaker::check_risk_limits(TreasuryType instrument, uint64_t order_size) const noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return false; // Invalid instrument
    }
    
    const auto& position = positions_[instrument_index];
    
    // Check position limits
    const int64_t new_long_position = position.net_position + static_cast<int64_t>(order_size);
    const int64_t new_short_position = position.net_position - static_cast<int64_t>(order_size);
    
    // Ensure we don't exceed maximum position in either direction
    return std::abs(new_long_position) <= MAX_POSITION_SIZE && 
           std::abs(new_short_position) <= MAX_POSITION_SIZE;
}

inline void SimpleMarketMaker::update_position(TreasuryType instrument, int64_t size_change, Price32nd fill_price) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return; // Invalid instrument
    }
    
    auto& position = positions_[instrument_index];
    
    // Calculate realized P&L for the trade
    const double fill_price_decimal = fill_price.to_decimal();
    const double previous_position = position.net_position;
    
    if ((previous_position > 0 && size_change < 0) || (previous_position < 0 && size_change > 0)) {
        // Closing or reducing position - calculate realized P&L
        const double closed_amount = std::min(static_cast<double>(std::abs(size_change)), std::abs(previous_position));
        const double realized_pnl = closed_amount * (fill_price_decimal - position.average_price) * 
                                   (previous_position > 0 ? 1.0 : -1.0);
        position.daily_pnl += realized_pnl;
    }
    
    // Update volume-weighted average price
    if ((position.net_position >= 0 && size_change > 0) || (position.net_position <= 0 && size_change < 0)) {
        // Increasing position in same direction
        const double total_value = position.net_position * position.average_price + size_change * fill_price_decimal;
        const double new_position = position.net_position + size_change;
        if (std::abs(new_position) > 1e-6) { // Avoid division by zero
            position.average_price = total_value / new_position;
        }
    }
    
    // Update net position
    position.net_position += size_change;
    
    // Update trade count
    ++position.trade_count;
    
    // Reset unrealized P&L (will be recalculated on next market update)
    position.unrealized_pnl = 0.0;
    
    // Update timestamp
    position.last_update_time = timer_.get_timestamp_ns();
    
    // Update risk score
    position.risk_score = calculate_position_risk_score(instrument);
}

// Phase 2: Implementation of enhanced methods

inline void SimpleMarketMaker::update_market_price(TreasuryType instrument, Price32nd current_price) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return;
    }
    
    auto& position = positions_[instrument_index];
    
    // Update unrealized P&L
    const double current_price_decimal = current_price.to_decimal();
    if (std::abs(position.net_position) > 1e-6) {
        position.unrealized_pnl = position.net_position * (current_price_decimal - position.average_price);
        position.daily_pnl = position.daily_pnl + position.unrealized_pnl; // Include unrealized in daily
    }
    
    // Update position VaR (simplified calculation)
    const double volatility = instrument_volatilities_[instrument_index];
    position.position_var = std::abs(position.net_position) * volatility * 2.33; // 99% VaR relative to position
}

inline void SimpleMarketMaker::update_portfolio_risk() noexcept {
    const uint64_t current_time = timer_.get_timestamp_ns();
    
    // Only update if enough time has passed (avoid excessive computation)
    if (current_time - last_risk_update_ns_ < POSITION_UPDATE_INTERVAL_NS) {
        return;
    }
    
    portfolio_var_ = calculate_portfolio_var();
    last_risk_update_ns_ = current_time;
}

inline uint32_t SimpleMarketMaker::calculate_position_risk_score(TreasuryType instrument) const noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return 1000; // Maximum risk for invalid instrument
    }
    
    const auto& position = positions_[instrument_index];
    
    // Risk score based on position size, volatility, and concentration
    const double position_ratio = std::abs(position.net_position) / static_cast<double>(MAX_POSITION_SIZE);
    const double volatility_factor = instrument_volatilities_[instrument_index] / 0.015; // Normalize to 30Y bond vol
    const double concentration_penalty = position_ratio > 0.5 ? (position_ratio - 0.5) * 2.0 : 0.0;
    
    const double risk_score = (position_ratio * 400.0) + (volatility_factor * 300.0) + (concentration_penalty * 300.0);
    
    return static_cast<uint32_t>(std::min(risk_score, 1000.0));
}

inline double SimpleMarketMaker::calculate_portfolio_var() const noexcept {
    double total_var = 0.0;
    
    // Simple portfolio VaR: sum of individual VaRs with correlation adjustment
    for (size_t i = 0; i < positions_.size(); ++i) {
        total_var += positions_[i].position_var;
    }
    
    // Apply correlation factor (simplified model)
    return total_var * CORRELATION_FACTOR;
}

inline std::pair<Price32nd, Price32nd> SimpleMarketMaker::apply_position_skewing(
    TreasuryType instrument,
    Price32nd base_bid,
    Price32nd base_ask
) const noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return {base_bid, base_ask}; // No skewing for invalid instrument
    }
    
    const auto& position = positions_[instrument_index];
    
    // No skewing if position is small
    if (std::abs(position.net_position) < MAX_POSITION_SIZE / 10) {
        return {base_bid, base_ask};
    }
    
    // Calculate skewing based on position
    const double position_ratio = position.net_position / static_cast<double>(MAX_POSITION_SIZE);
    const double skew_factor = std::min(std::abs(position_ratio), 1.0 / MAX_SKEW_RATIO);
    
    const double base_bid_decimal = base_bid.to_decimal();
    const double base_ask_decimal = base_ask.to_decimal();
    const double spread = base_ask_decimal - base_bid_decimal;
    const double skew_adjustment = spread * skew_factor * SKEW_PENALTY_BPS / 10000.0;
    
    Price32nd skewed_bid, skewed_ask;
    
    if (position.net_position > 0) {
        // Long position: widen ask, tighten bid to encourage selling
        skewed_bid = Price32nd::from_decimal(base_bid_decimal + skew_adjustment);
        skewed_ask = Price32nd::from_decimal(base_ask_decimal + skew_adjustment * 2.0);
    } else {
        // Short position: widen bid, tighten ask to encourage buying
        skewed_bid = Price32nd::from_decimal(base_bid_decimal - skew_adjustment * 2.0);
        skewed_ask = Price32nd::from_decimal(base_ask_decimal - skew_adjustment);
    }
    
    return {skewed_bid, skewed_ask};
}

inline bool SimpleMarketMaker::check_enhanced_risk_limits(TreasuryType instrument, uint64_t order_size) const noexcept {
    // Check basic position limits first
    if (!check_risk_limits(instrument, order_size)) {
        return false;
    }
    
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= positions_.size()) {
        return false;
    }
    
    const auto& position = positions_[instrument_index];
    
    // Check portfolio VaR limit (only if it has been calculated)
    if (portfolio_var_ > MAX_PORTFOLIO_VAR && portfolio_var_ > 0.0) {
        return false;
    }
    
    // Check individual position risk score
    if (position.risk_score > 900) { // 90% of maximum risk
        return false;
    }
    
    // Check if adding this order would exceed volatility thresholds
    const double volatility = instrument_volatilities_[instrument_index];
    if (volatility > VOLATILITY_THRESHOLD && std::abs(position.net_position) > MAX_POSITION_SIZE / 2) {
        return false;
    }
    
    return true;
}

// Static asserts for complete type
static_assert(alignof(SimpleMarketMaker) == CACHE_LINE_SIZE, "SimpleMarketMaker must be cache-aligned");
static_assert(alignof(SimpleMarketMaker::TradingDecision) == CACHE_LINE_SIZE, "TradingDecision must be cache-aligned");

} // namespace strategy
} // namespace hft