#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <cmath>
#include <array>
#include <algorithm>
#include <string_view>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

namespace hft {
namespace market_data {

// ARM64 cache line size
constexpr size_t CACHE_LINE_SIZE = 64;

// Treasury instrument types
enum class TreasuryType : uint8_t {
    Bill_3M,
    Bill_6M,
    Note_2Y,
    Note_5Y,
    Note_10Y,
    Bond_30Y
};

// Treasury instrument definition
struct alignas(CACHE_LINE_SIZE) TreasuryInstrument {
    TreasuryType type;                // 1
    uint8_t _pad0[3];                 // 3 (align to 4)
    uint32_t maturity_days;           // 4
    uint64_t face_value;              // 8
    uint8_t _pad1[48];                // 48 (total: 1+3+4+8+48 = 64)
};
static_assert(sizeof(TreasuryInstrument) == CACHE_LINE_SIZE, "TreasuryInstrument must be 64 bytes");

// 32nd fractional pricing (e.g., 99-16.5 = 99.515625)
struct Price32nd {
    uint16_t whole;         // 2
    uint8_t thirty_seconds; // 1
    uint8_t half_32nds;     // 1
    uint8_t _pad[4];        // 4 (total: 2+1+1+4 = 8)
    constexpr double to_decimal() const noexcept {
        return static_cast<double>(whole) +
               static_cast<double>(thirty_seconds) / 32.0 +
               static_cast<double>(half_32nds) / 64.0;
    }
    static constexpr Price32nd from_decimal(double price) noexcept {
        uint16_t w = static_cast<uint16_t>(price);
        double frac = price - static_cast<double>(w);
        uint8_t t32 = static_cast<uint8_t>(frac * 32.0);
        double rem = frac * 32.0 - static_cast<double>(t32);
        uint8_t h32 = (rem >= 0.5) ? 1 : 0;
        return Price32nd{w, t32, h32, {0,0,0,0}};
    }
};
static_assert(sizeof(Price32nd) == 8, "Price32nd must be 8 bytes");

// Ultra-fast yield calculations (target: <100ns)
class YieldCalculator {
public:
    // Price-to-yield conversion using Newton-Raphson (Actual/Actual)
    static double price_to_yield(const TreasuryInstrument& instrument, Price32nd price, uint32_t days_to_maturity) noexcept {
        constexpr double FACE = 1'000'000.0;
        double p = price.to_decimal() / 100.0 * FACE;
        double y = 0.02; // Initial guess (2%)
        double t = static_cast<double>(days_to_maturity) / 365.0;
        constexpr int MAX_ITER = 8;
        constexpr double EPS = 1e-8;
        for (int i = 0; i < MAX_ITER; ++i) {
            double denom = 1.0 + y * t;
            double f = FACE / denom - p;
            double df = -FACE * t / (denom * denom);
            double delta = f / df;
            y -= delta;
            if (std::abs(delta) < EPS) break;
        }
        return std::round(y * 10000.0) / 10000.0; // 4 decimal places
    }
    // Yield-to-price conversion
    static Price32nd yield_to_price(const TreasuryInstrument& instrument, double yield, uint32_t days_to_maturity) noexcept {
        constexpr double FACE = 1'000'000.0;
        double t = static_cast<double>(days_to_maturity) / 365.0;
        double denom = 1.0 + yield * t;
        double price = (FACE / denom) / FACE * 100.0;
        return Price32nd::from_decimal(price);
    }
};

// Market data tick integrated with SPSC ring buffers
struct alignas(CACHE_LINE_SIZE) TreasuryTick {
    TreasuryType instrument_type;                // 1
    uint8_t _pad0[7];                           // 7 (align to 8)
    hft::HFTTimer::timestamp_t timestamp_ns;    // 8
    Price32nd bid_price;                        // 8
    Price32nd ask_price;                        // 8
    uint64_t bid_size;                          // 8
    uint64_t ask_size;                          // 8
    double bid_yield;                           // 8
    double ask_yield;                           // 8
    // 1+7+8+8+8+8+8+8+8 = 64
    bool is_valid() const noexcept {
        return bid_price.whole > 0 && ask_price.whole > 0 && bid_size > 0 && ask_size > 0;
    }
};
static_assert(sizeof(TreasuryTick) == CACHE_LINE_SIZE, "TreasuryTick must be 64 bytes");

// Trade message
struct alignas(CACHE_LINE_SIZE) TreasuryTrade {
    TreasuryType instrument_type;                // 1
    uint8_t _pad0[7];                           // 7 (align to 8)
    hft::HFTTimer::timestamp_t timestamp_ns;    // 8
    Price32nd trade_price;                      // 8
    uint64_t trade_size;                        // 8
    double trade_yield;                         // 8
    char trade_id[16];                          // 16
    uint8_t _pad1[8];                           // 8 (total: 1+7+8+8+8+8+16+8 = 64)
};
static_assert(sizeof(TreasuryTrade) == CACHE_LINE_SIZE, "TreasuryTrade must be 64 bytes");

// Object pool integration for market data messages
using TreasuryTickPool = hft::ObjectPool<TreasuryTick, 4096, false>;
using TreasuryTradePool = hft::ObjectPool<TreasuryTrade, 4096, false>;

// Ring buffer integration for market data feeds
using TreasuryTickBuffer = hft::SPSCRingBuffer<TreasuryTick, 8192>;
using TreasuryTradeBuffer = hft::SPSCRingBuffer<TreasuryTrade, 8192>;

// Market data processor with timing integration
class TreasuryMarketDataProcessor {
public:
    TreasuryMarketDataProcessor() noexcept : total_ticks_(0), total_latency_ns_(0) {}

    // Process incoming tick with latency measurement
    bool process_tick(const TreasuryTick& tick) noexcept {
        auto start = hft::HFTTimer::get_cycles();
        // Fast-path: validate and process
        if (__builtin_expect(tick.is_valid(), 1)) {
            ++total_ticks_;
            // Simulate processing (no-op)
            auto end = hft::HFTTimer::get_cycles();
            total_latency_ns_ += hft::HFTTimer::cycles_to_ns(end - start);
            return true;
        }
        return false;
    }

    // Batch processing for high throughput
    template<typename Iterator>
    size_t process_tick_batch(Iterator begin, Iterator end) noexcept {
        size_t count = 0;
        auto batch_start = hft::HFTTimer::get_cycles();
        for (auto it = begin; it != end; ++it) {
            if (process_tick(*it)) ++count;
        }
        auto batch_end = hft::HFTTimer::get_cycles();
        total_latency_ns_ += hft::HFTTimer::cycles_to_ns(batch_end - batch_start);
        return count;
    }

    // Get processing statistics
    hft::HFTTimer::ns_t get_avg_processing_latency() const noexcept {
        return total_ticks_ ? total_latency_ns_ / total_ticks_ : 0;
    }

private:
    size_t total_ticks_;
    hft::HFTTimer::ns_t total_latency_ns_;
};

// Static asserts for type sizes and alignments
static_assert(alignof(TreasuryInstrument) == CACHE_LINE_SIZE, "TreasuryInstrument alignment");
static_assert(alignof(TreasuryTick) == CACHE_LINE_SIZE, "TreasuryTick alignment");
static_assert(alignof(TreasuryTrade) == CACHE_LINE_SIZE, "TreasuryTrade alignment");

} // namespace market_data
} // namespace hft 