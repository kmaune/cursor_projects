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
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/trading/order_book.hpp"

// Include common constants to avoid redefinitions
namespace {
    constexpr size_t CACHE_LINE_SIZE = 64;
    constexpr uint32_t TREASURY_32NDS_DENOMINATOR = 32;
}

namespace hft {
namespace strategy {

using namespace hft::market_data;
using namespace hft::trading;

// Treasury pricing constants (use common constants from integrated_market_making_strategy.hpp)
constexpr double MIN_32NDS_INCREMENT = 1.0 / 32.0;                    // 1/32nd
constexpr double HALF_32NDS_INCREMENT = 1.0 / 64.0;                   // 1/64th (0.5/32nds)
constexpr uint64_t MIN_QUOTE_SIZE = 100000;                           // $100k minimum
constexpr uint64_t MAX_QUOTE_SIZE = 50000000;                         // $50M maximum
constexpr double MIN_SPREAD_32NDS = 1.0 / 32.0;                       // Minimum 1/32nd spread

// Quote update thresholds to minimize market churn
constexpr double PRICE_UPDATE_THRESHOLD_32NDS = 0.5 / 32.0;           // 0.5/32nds
constexpr double SIZE_UPDATE_THRESHOLD_PERCENT = 0.1;                 // 10% size change
constexpr uint64_t MAX_QUOTE_UPDATE_FREQUENCY_NS = 100000000;         // Max 10 updates/sec per instrument

// Quote state for a single instrument
struct alignas(64) InstrumentQuoteState {
    TreasuryType instrument;                          // 1 byte
    uint8_t _pad0[7];                                // 7 bytes padding
    
    // Current active quotes
    std::atomic<Price32nd> active_bid_price;         // 8 bytes
    std::atomic<Price32nd> active_ask_price;         // 8 bytes
    std::atomic<uint64_t> active_bid_size;           // 8 bytes
    std::atomic<uint64_t> active_ask_size;           // 8 bytes
    
    // Quote management state
    std::atomic<uint64_t> bid_order_id;              // 8 bytes
    std::atomic<uint64_t> ask_order_id;              // 8 bytes
    std::atomic<hft::HFTTimer::timestamp_t> last_update_ns;  // 8 bytes
    std::atomic<uint32_t> update_count_today;        // 4 bytes
    uint8_t _pad1[4];                                // 4 bytes padding
    // Total: 64 bytes exactly
    
    InstrumentQuoteState() noexcept : instrument(TreasuryType::Note_10Y) {
        std::memset(_pad0, 0, sizeof(_pad0));
        std::memset(_pad1, 0, sizeof(_pad1));
        active_bid_price.store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
        active_ask_price.store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
        active_bid_size.store(0, std::memory_order_relaxed);
        active_ask_size.store(0, std::memory_order_relaxed);
        bid_order_id.store(0, std::memory_order_relaxed);
        ask_order_id.store(0, std::memory_order_relaxed);
        last_update_ns.store(0, std::memory_order_relaxed);
        update_count_today.store(0, std::memory_order_relaxed);
    }
    
    explicit InstrumentQuoteState(TreasuryType instr) noexcept : InstrumentQuoteState() {
        instrument = instr;
    }
};

// Quote validation result
enum class QuoteValidationResult : uint8_t {
    VALID = 0,
    INVALID_PRICE_INCREMENT,     // Price not aligned to 32nds
    INVALID_SPREAD,              // Spread too narrow
    INVALID_SIZE,                // Size outside limits
    INVALID_PRICE_ORDER,         // Bid >= Ask
    UPDATE_NOT_NEEDED,           // Change too small to warrant update
    RATE_LIMITED                 // Too many updates recently
};

// Treasury quote update request
struct alignas(32) QuoteUpdateRequest {
    TreasuryType instrument;                         // 1 byte
    uint8_t _pad0[7];                               // 7 bytes padding
    Price32nd new_bid_price;                        // 8 bytes
    Price32nd new_ask_price;                        // 8 bytes
    uint64_t new_bid_size;                          // 8 bytes
    uint64_t new_ask_size;                          // 8 bytes
    hft::HFTTimer::timestamp_t request_time_ns;     // 8 bytes
    uint64_t request_id;                            // 8 bytes
    // Total: 56 bytes (cache-friendly)
    
    QuoteUpdateRequest() noexcept : instrument(TreasuryType::Note_10Y) {
        std::memset(_pad0, 0, sizeof(_pad0));
    }
    
    QuoteUpdateRequest(TreasuryType instr, Price32nd bid, Price32nd ask, 
                      uint64_t bid_size, uint64_t ask_size) noexcept
        : instrument(instr), new_bid_price(bid), new_ask_price(ask),
          new_bid_size(bid_size), new_ask_size(ask_size),
          request_time_ns(hft::HFTTimer::get_timestamp_ns()), request_id(0) {
        std::memset(_pad0, 0, sizeof(_pad0));
    }
};

// Main treasury quote manager class
class TreasuryQuoteManager {
private:
    // Infrastructure references
    hft::memory::ObjectPool<TreasuryOrder, 1000>& order_pool_;
    TreasuryOrderBook& order_book_;
    
    // Quote state per instrument (cache-aligned)
    alignas(64) std::array<InstrumentQuoteState, 6> quote_states_;
    
    // Performance tracking
    hft::HFTTimer timer_;
    mutable std::atomic<uint64_t> quote_updates_processed_;
    mutable std::atomic<uint64_t> total_quote_latency_ns_;
    mutable std::atomic<uint64_t> validation_failures_;
    
    // Next order ID generation
    std::atomic<uint64_t> next_order_id_;
    
public:
    explicit TreasuryQuoteManager(hft::memory::ObjectPool<TreasuryOrder, 1000>& order_pool,
                                TreasuryOrderBook& order_book) noexcept
        : order_pool_(order_pool)
        , order_book_(order_book)
        , timer_()
        , quote_updates_processed_(0)
        , total_quote_latency_ns_(0)
        , validation_failures_(0)
        , next_order_id_(1) {
        
        // Initialize quote states for each instrument (cannot assign due to atomic members)
        for (size_t i = 0; i < 6; ++i) {
            auto& state = quote_states_[i];
            state.instrument = static_cast<TreasuryType>(i);
            state.active_bid_price.store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
            state.active_ask_price.store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
            state.active_bid_size.store(0, std::memory_order_relaxed);
            state.active_ask_size.store(0, std::memory_order_relaxed);
            state.bid_order_id.store(0, std::memory_order_relaxed);
            state.ask_order_id.store(0, std::memory_order_relaxed);
            state.last_update_ns.store(0, std::memory_order_relaxed);
            state.update_count_today.store(0, std::memory_order_relaxed);
        }
    }
    
    // Delete copy/move constructors to ensure single instance
    TreasuryQuoteManager(const TreasuryQuoteManager&) = delete;
    TreasuryQuoteManager& operator=(const TreasuryQuoteManager&) = delete;
    TreasuryQuoteManager(TreasuryQuoteManager&&) = delete;
    TreasuryQuoteManager& operator=(TreasuryQuoteManager&&) = delete;
    
    // Validate treasury quote for 32nds compliance and market rules
    QuoteValidationResult validate_quote(const QuoteUpdateRequest& request) const noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        // Check 32nds price alignment (target: 20ns)
        if (!is_32nds_aligned(request.new_bid_price) || !is_32nds_aligned(request.new_ask_price)) {
            return QuoteValidationResult::INVALID_PRICE_INCREMENT;
        }
        
        // Check bid/ask order (target: 10ns)
        if (request.new_bid_price.to_decimal() >= request.new_ask_price.to_decimal()) {
            return QuoteValidationResult::INVALID_PRICE_ORDER;
        }
        
        // Check minimum spread (target: 10ns)
        const double spread = request.new_ask_price.to_decimal() - request.new_bid_price.to_decimal();
        if (spread < MIN_SPREAD_32NDS) {
            return QuoteValidationResult::INVALID_SPREAD;
        }
        
        // Check size limits (target: 10ns)
        if (request.new_bid_size < MIN_QUOTE_SIZE || request.new_bid_size > MAX_QUOTE_SIZE ||
            request.new_ask_size < MIN_QUOTE_SIZE || request.new_ask_size > MAX_QUOTE_SIZE) {
            return QuoteValidationResult::INVALID_SIZE;
        }
        
        const auto instrument_index = static_cast<size_t>(request.instrument);
        const auto& quote_state = quote_states_[instrument_index];
        
        // Check rate limiting (target: 20ns)
        const auto last_update = quote_state.last_update_ns.load(std::memory_order_relaxed);
        if (request.request_time_ns - last_update < MAX_QUOTE_UPDATE_FREQUENCY_NS) {
            return QuoteValidationResult::RATE_LIMITED;
        }
        
        // Check if update is significant enough (target: 30ns)
        const auto current_bid = quote_state.active_bid_price.load(std::memory_order_relaxed);
        const auto current_ask = quote_state.active_ask_price.load(std::memory_order_relaxed);
        const auto current_bid_size = quote_state.active_bid_size.load(std::memory_order_relaxed);
        const auto current_ask_size = quote_state.active_ask_size.load(std::memory_order_relaxed);
        
        const double bid_price_change = std::abs(request.new_bid_price.to_decimal() - current_bid.to_decimal());
        const double ask_price_change = std::abs(request.new_ask_price.to_decimal() - current_ask.to_decimal());
        
        const double bid_size_change = current_bid_size > 0 ? 
            std::abs(static_cast<double>(request.new_bid_size) - current_bid_size) / current_bid_size : 1.0;
        const double ask_size_change = current_ask_size > 0 ? 
            std::abs(static_cast<double>(request.new_ask_size) - current_ask_size) / current_ask_size : 1.0;
        
        if (bid_price_change < PRICE_UPDATE_THRESHOLD_32NDS && 
            ask_price_change < PRICE_UPDATE_THRESHOLD_32NDS &&
            bid_size_change < SIZE_UPDATE_THRESHOLD_PERCENT &&
            ask_size_change < SIZE_UPDATE_THRESHOLD_PERCENT) {
            return QuoteValidationResult::UPDATE_NOT_NEEDED;
        }
        
        return QuoteValidationResult::VALID;
    }
    
    // Process quote update with atomic state management (target: 300ns total)
    bool process_quote_update(const QuoteUpdateRequest& request) noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        // Validate quote (target: 100ns)
        const auto validation_result = validate_quote(request);
        if (validation_result != QuoteValidationResult::VALID) {
            validation_failures_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        const auto instrument_index = static_cast<size_t>(request.instrument);
        auto& quote_state = quote_states_[instrument_index];
        
        // Cancel existing orders if any (target: 100ns)
        const auto current_bid_order_id = quote_state.bid_order_id.load(std::memory_order_relaxed);
        const auto current_ask_order_id = quote_state.ask_order_id.load(std::memory_order_relaxed);
        
        if (current_bid_order_id != 0) {
            order_book_.cancel_order(current_bid_order_id);
        }
        if (current_ask_order_id != 0) {
            order_book_.cancel_order(current_ask_order_id);
        }
        
        // Place new orders (target: 100ns)
        const auto bid_order_id = next_order_id_.fetch_add(1, std::memory_order_relaxed);
        const auto ask_order_id = next_order_id_.fetch_add(1, std::memory_order_relaxed);
        
        // Get orders from pool
        auto bid_order = order_pool_.acquire();
        auto ask_order = order_pool_.acquire();
        
        if (!bid_order || !ask_order) {
            // Pool exhausted - return orders if we got any
            if (bid_order) order_pool_.release(bid_order);
            if (ask_order) order_pool_.release(ask_order);
            return false;
        }
        
        // Initialize bid order
        *bid_order = TreasuryOrder{};
        bid_order->order_id = bid_order_id;
        bid_order->instrument_type = request.instrument;
        bid_order->side = OrderSide::BID;
        bid_order->type = OrderType::LIMIT;
        bid_order->price = request.new_bid_price;
        bid_order->quantity = request.new_bid_size;
        bid_order->remaining_quantity = request.new_bid_size;
        bid_order->timestamp_ns = timer_.get_timestamp_ns();
        bid_order->sequence_number = bid_order_id; // Use order ID as sequence
        
        // Initialize ask order
        *ask_order = TreasuryOrder{};
        ask_order->order_id = ask_order_id;
        ask_order->instrument_type = request.instrument;
        ask_order->side = OrderSide::ASK;
        ask_order->type = OrderType::LIMIT;
        ask_order->price = request.new_ask_price;
        ask_order->quantity = request.new_ask_size;
        ask_order->remaining_quantity = request.new_ask_size;
        ask_order->timestamp_ns = timer_.get_timestamp_ns();
        ask_order->sequence_number = ask_order_id; // Use order ID as sequence
        
        // Add orders to book
        const bool bid_added = order_book_.add_order(*bid_order);
        const bool ask_added = order_book_.add_order(*ask_order);
        
        if (!bid_added || !ask_added) {
            // Failed to add orders - clean up
            if (bid_added) order_book_.cancel_order(bid_order_id);
            if (ask_added) order_book_.cancel_order(ask_order_id);
            order_pool_.release(bid_order);
            order_pool_.release(ask_order);
            return false;
        }
        
        // Update quote state atomically
        quote_state.active_bid_price.store(request.new_bid_price, std::memory_order_relaxed);
        quote_state.active_ask_price.store(request.new_ask_price, std::memory_order_relaxed);
        quote_state.active_bid_size.store(request.new_bid_size, std::memory_order_relaxed);
        quote_state.active_ask_size.store(request.new_ask_size, std::memory_order_relaxed);
        quote_state.bid_order_id.store(bid_order_id, std::memory_order_relaxed);
        quote_state.ask_order_id.store(ask_order_id, std::memory_order_relaxed);
        quote_state.last_update_ns.store(timer_.get_timestamp_ns(), std::memory_order_relaxed);
        quote_state.update_count_today.fetch_add(1, std::memory_order_relaxed);
        
        // Update performance tracking
        const auto end_time = timer_.get_timestamp_ns();
        const auto total_latency = end_time - start_time;
        
        quote_updates_processed_.fetch_add(1, std::memory_order_relaxed);
        total_quote_latency_ns_.fetch_add(total_latency, std::memory_order_relaxed);
        
        return true;
    }
    
    // Cancel all quotes for an instrument (target: 100ns)
    bool cancel_quotes(TreasuryType instrument) noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        auto& quote_state = quote_states_[instrument_index];
        
        const auto bid_order_id = quote_state.bid_order_id.load(std::memory_order_relaxed);
        const auto ask_order_id = quote_state.ask_order_id.load(std::memory_order_relaxed);
        
        bool success = true;
        
        if (bid_order_id != 0) {
            success &= order_book_.cancel_order(bid_order_id);
            quote_state.bid_order_id.store(0, std::memory_order_relaxed);
        }
        
        if (ask_order_id != 0) {
            success &= order_book_.cancel_order(ask_order_id);
            quote_state.ask_order_id.store(0, std::memory_order_relaxed);
        }
        
        // Clear quote state
        quote_state.active_bid_price.store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
        quote_state.active_ask_price.store(Price32nd::from_decimal(0.0), std::memory_order_relaxed);
        quote_state.active_bid_size.store(0, std::memory_order_relaxed);
        quote_state.active_ask_size.store(0, std::memory_order_relaxed);
        
        return success;
    }
    
    // Cancel all quotes for all instruments (emergency flatten)
    void cancel_all_quotes() noexcept {
        for (size_t i = 0; i < 6; ++i) {
            cancel_quotes(static_cast<TreasuryType>(i));
        }
    }
    
    // Get current quote for an instrument (for monitoring)
    struct CurrentQuote {
        Price32nd bid_price;
        Price32nd ask_price;
        uint64_t bid_size;
        uint64_t ask_size;
        uint64_t bid_order_id;
        uint64_t ask_order_id;
        hft::HFTTimer::timestamp_t last_update_ns;
        bool is_active;
    };
    
    CurrentQuote get_current_quote(TreasuryType instrument) const noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        const auto& quote_state = quote_states_[instrument_index];
        
        CurrentQuote quote{};
        quote.bid_price = quote_state.active_bid_price.load(std::memory_order_relaxed);
        quote.ask_price = quote_state.active_ask_price.load(std::memory_order_relaxed);
        quote.bid_size = quote_state.active_bid_size.load(std::memory_order_relaxed);
        quote.ask_size = quote_state.active_ask_size.load(std::memory_order_relaxed);
        quote.bid_order_id = quote_state.bid_order_id.load(std::memory_order_relaxed);
        quote.ask_order_id = quote_state.ask_order_id.load(std::memory_order_relaxed);
        quote.last_update_ns = quote_state.last_update_ns.load(std::memory_order_relaxed);
        quote.is_active = (quote.bid_order_id != 0 && quote.ask_order_id != 0);
        
        return quote;
    }
    
    // Performance statistics
    struct QuoteManagerStats {
        uint64_t total_updates_processed;
        double average_update_latency_ns;
        uint64_t validation_failures;
        double update_success_rate;
        std::array<uint32_t, 6> updates_per_instrument;
    };
    
    QuoteManagerStats get_performance_stats() const noexcept {
        const auto total_updates = quote_updates_processed_.load(std::memory_order_relaxed);
        const auto total_latency = total_quote_latency_ns_.load(std::memory_order_relaxed);
        const auto failures = validation_failures_.load(std::memory_order_relaxed);
        
        QuoteManagerStats stats{};
        stats.total_updates_processed = total_updates;
        stats.average_update_latency_ns = total_updates > 0 ? 
            static_cast<double>(total_latency) / total_updates : 0.0;
        stats.validation_failures = failures;
        stats.update_success_rate = (total_updates + failures) > 0 ? 
            static_cast<double>(total_updates) / (total_updates + failures) : 0.0;
        
        for (size_t i = 0; i < 6; ++i) {
            stats.updates_per_instrument[i] = quote_states_[i].update_count_today.load(std::memory_order_relaxed);
        }
        
        return stats;
    }
    
private:
    // Check if price is aligned to 32nds increments
    static bool is_32nds_aligned(Price32nd price) noexcept {
        const double price_double = price.to_decimal();
        const double scaled = price_double * TREASURY_32NDS_DENOMINATOR;
        const double rounded = std::round(scaled);
        return std::abs(scaled - rounded) < 1e-10; // Floating point tolerance
    }
    
    // Round price to nearest 32nd
    static Price32nd round_to_32nds(double price) noexcept {
        const double scaled = price * TREASURY_32NDS_DENOMINATOR;
        const double rounded = std::round(scaled);
        return Price32nd::from_decimal(rounded / TREASURY_32NDS_DENOMINATOR);
    }
};

} // namespace strategy
} // namespace hft