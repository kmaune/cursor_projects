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
          positions_{}, decision_count_(0), total_decision_latency_ns_(0) {
        
        // Initialize positions to zero
        for (auto& pos : positions_) {
            pos.net_position = 0;
            pos.unrealized_pnl = 0.0;
            pos.last_update_time = 0;
        }
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

private:
    // Configuration constants
    static constexpr double BASE_SPREAD_BPS = 0.5;           // 0.5 basis points base spread
    static constexpr double INVENTORY_PENALTY_BPS = 1.0;     // 1.0bp per $10M position
    static constexpr int64_t MAX_POSITION_SIZE = 50000000;   // $50M position limit
    static constexpr int64_t BASE_QUOTE_SIZE = 1000000;      // $1M base quote size
    static constexpr uint64_t TARGET_DECISION_NS = 1000;     // 1μs target latency
    static constexpr double MIN_SPREAD_BPS = 0.25;           // Minimum 0.25bp spread

    // Position tracking structure
    struct alignas(CACHE_LINE_SIZE) Position {
        int64_t net_position = 0;                           // Net position in dollars
        double unrealized_pnl = 0.0;                       // Mark-to-market P&L
        uint64_t last_update_time = 0;                     // Last update timestamp
        uint8_t _pad[40];                                   // Padding to cache line
        
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
        // Step 2: Risk checks (target: 100ns)
        if (check_risk_limits(update.instrument, BASE_QUOTE_SIZE)) {
            // Step 3: Calculate spread based on inventory (target: 200ns)
            const double spread_bps = calculate_spread_bps(update.instrument);
            
            // Step 4: Generate quotes (target: 300ns)
            const auto [bid_price, ask_price] = calculate_quotes(update, spread_bps);
            
            // Step 5: Size calculation (target: 100ns)
            const uint64_t quote_size = calculate_quote_size(update.instrument);
            
            // Step 6: Final decision assembly (target: 50ns)
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
    
    // Update net position
    position.net_position += size_change;
    
    // Simple mark-to-market: assume current price equals fill price for now
    // In a full implementation, this would use current market prices
    position.unrealized_pnl = 0.0; // Reset to zero after trade
    
    // Update timestamp
    position.last_update_time = timer_.get_timestamp_ns();
}

// Static asserts for complete type
static_assert(alignof(SimpleMarketMaker) == CACHE_LINE_SIZE, "SimpleMarketMaker must be cache-aligned");
static_assert(alignof(SimpleMarketMaker::TradingDecision) == CACHE_LINE_SIZE, "TradingDecision must be cache-aligned");

} // namespace strategy
} // namespace hft