# Treasury Market Data Structures - Cursor Prompt

Create a high-performance Treasury market data structure system for our HFT trading system as a single header file.

## Requirements
- **Single header implementation:** `include/hft/market_data/treasury_instruments.hpp`
- **Treasury instruments:** 2Y, 5Y, 10Y Treasury Notes + 3M, 6M Bills + 30Y Bonds
- **Precision pricing:** 32nd fractional representation + yield to 4 decimal places
- **ARM64 cache-line optimization:** 64-byte alignment for all data structures
- **Zero allocation in hot paths:** All calculations and updates lock-free
- **Integration with existing HFTTimer:** Nanosecond-precision timing for yield calculations
- **Template-based optimization:** Compile-time instrument type dispatch

## Core Data Structures Needed

### 1. Treasury Instrument Definition
```cpp
enum class TreasuryType : uint8_t { Bill_3M, Bill_6M, Note_2Y, Note_5Y, Note_10Y, Bond_30Y };

struct alignas(64) TreasuryInstrument {
    TreasuryType type;
    uint32_t maturity_days;
    uint64_t face_value;  // Always $1,000,000 for institutional
    // 32nd pricing: price in 32nds (e.g., 99-16 = 99.5)
    // Cache-aligned fields for hot path access
};
```

### 2. Precision Pricing & Yield Calculations
```cpp
// 32nd fractional pricing (e.g., 99-16.5 = 99.515625)
struct Price32nd {
    uint32_t whole;      // Whole number part (99)
    uint32_t thirty_seconds; // 32nds part (16)
    uint32_t half_32nds; // Half 32nds (0 or 1)
    
    // Fast conversion methods
    double to_decimal() const noexcept;
    static Price32nd from_decimal(double price) noexcept;
};

// Ultra-fast yield calculations (target: <100ns)
class YieldCalculator {
    // Price-to-yield conversion using Newton-Raphson method
    static double price_to_yield(const TreasuryInstrument& instrument, 
                                Price32nd price, uint32_t days_to_maturity) noexcept;
    
    // Yield-to-price conversion
    static Price32nd yield_to_price(const TreasuryInstrument& instrument,
                                   double yield, uint32_t days_to_maturity) noexcept;
};
```

### 3. Market Data Message Types
```cpp
// Market data tick integrated with our SPSC ring buffers
struct alignas(64) TreasuryTick {
    TreasuryType instrument_type;
    HFTTimer::timestamp_t timestamp_ns;
    Price32nd bid_price;
    Price32nd ask_price;
    uint64_t bid_size;    // In millions
    uint64_t ask_size;    // In millions
    double bid_yield;     // Calculated yield (4 decimal precision)
    double ask_yield;     // Calculated yield (4 decimal precision)
    
    // Integration point for ring buffer
    bool is_valid() const noexcept;
};

// Trade message
struct alignas(64) TreasuryTrade {
    TreasuryType instrument_type;
    HFTTimer::timestamp_t timestamp_ns;
    Price32nd trade_price;
    uint64_t trade_size;
    double trade_yield;
    char trade_id[16];    // Exchange trade ID
};
```

### 4. Performance Integration Points
```cpp
// Object pool integration for market data messages
using TreasuryTickPool = ObjectPool<TreasuryTick, 4096, false>;
using TreasuryTradePool = ObjectPool<TreasuryTrade, 4096, false>;

// Ring buffer integration for market data feeds
using TreasuryTickBuffer = SPSCRingBuffer<TreasuryTick, 8192>;
using TreasuryTradeBuffer = SPSCRingBuffer<TreasuryTrade, 8192>;

// Market data processor with timing integration
class TreasuryMarketDataProcessor {
public:
    // Process incoming tick with latency measurement
    bool process_tick(const TreasuryTick& tick) noexcept;
    
    // Batch processing for high throughput
    template<typename Iterator>
    size_t process_tick_batch(Iterator begin, Iterator end) noexcept;
    
    // Get processing statistics
    HFTTimer::ns_t get_avg_processing_latency() const noexcept;
};
```

## Performance Requirements (HFT Critical)
- **Yield calculations:** <100ns per calculation
- **Price conversions:** <20ns (32nd to decimal and back)
- **Market data processing:** <50ns per tick
- **Memory access:** All hot path data within single cache line
- **Branch prediction:** Use likely/unlikely hints for common paths
- **SIMD optimization:** Consider ARM64 NEON for batch calculations

## API Design Patterns
- **Static asserts:** Validate all type sizes and alignments
- **Constexpr functions:** Compile-time calculation where possible
- **Template specialization:** Per-instrument-type optimizations
- **Memory prefetching:** Batch processing optimizations
- **Error codes:** No exceptions in hot paths, return error codes

## Integration Requirements
- **HFTTimer integration:** All calculations timed and measured
- **Object pool compatibility:** Work with existing pool templates
- **Ring buffer compatibility:** Fit existing SPSC buffer templates
- **CMake integration:** Header-only library with proper dependencies

## Testing Requirements
Include comprehensive unit tests and benchmarks:
- **Precision tests:** 32nd pricing accuracy validation
- **Performance benchmarks:** Yield calculation latency measurement
- **Integration tests:** Object pool and ring buffer compatibility
- **Stress tests:** High-frequency processing validation
- **Edge cases:** Handle market close, holidays, settlement dates

## Treasury Market Specifics
- **Settlement:** T+1 for treasury securities
- **Day count:** Actual/Actual for yield calculations
- **Price quotes:** Always in 32nds for institutional markets
- **Yield precision:** 4 decimal places (0.0001 = 1 basis point)
- **Size conventions:** In millions of face value

Generate complete single-header implementation with comprehensive benchmarks and tests, following our proven HFT optimization patterns.
