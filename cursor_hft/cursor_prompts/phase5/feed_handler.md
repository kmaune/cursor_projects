# Feed Handler Framework - High-Performance Market Data Processing

Create a high-performance feed handler framework for processing treasury market data messages as a single header file.

## Requirements
- **Single header implementation:** `include/hft/market_data/feed_handler.hpp`
- **Message parsing:** Convert raw bytes to TreasuryTick/TreasuryTrade messages
- **Zero allocation:** All processing in pre-allocated buffers
- **ARM64 optimization:** 64-byte cache alignment, prefetching
- **Integration:** Seamless with existing TreasuryTick/TreasuryTrade structures
- **Performance monitoring:** Built-in latency measurement with HFTTimer
- **Quality controls:** Message validation, sequence checking, duplicate detection

## Core Components Needed

### 1. Message Format Definitions
```cpp
// Raw market data message from exchange feed
struct alignas(64) RawMarketMessage {
    uint64_t sequence_number;        // Message sequence from exchange
    uint64_t timestamp_exchange_ns;  // Exchange timestamp
    uint32_t message_type;           // Tick, Trade, etc.
    uint32_t instrument_id;          // Maps to TreasuryType
    char raw_data[32];               // Raw price/size data
    uint16_t checksum;               // Message integrity
    uint8_t _pad[14];                // Cache align to 64 bytes
};

// Parsed message metadata
enum class MessageType : uint32_t {
    Tick = 1,
    Trade = 2,
    Heartbeat = 3,
    Invalid = 0
};

// Message validation results
enum class ValidationResult : uint8_t {
    Valid,
    InvalidChecksum,
    InvalidSequence,
    InvalidFormat,
    Duplicate
};
```

### 2. High-Performance Message Parser
```cpp
// Template-based parser for different message types
template<typename OutputType>
class MessageParser {
public:
    // Parse raw message into treasury data structure
    static ValidationResult parse_message(
        const RawMarketMessage& raw_msg,
        OutputType& parsed_msg,
        HFTTimer::ns_t& parse_latency_ns) noexcept;
    
    // Batch parsing for high throughput
    template<typename InputIterator, typename OutputIterator>
    static size_t parse_batch(
        InputIterator raw_begin, InputIterator raw_end,
        OutputIterator parsed_begin, OutputIterator parsed_end,
        size_t& invalid_count) noexcept;
    
private:
    // Fast checksum validation
    static bool validate_checksum(const RawMarketMessage& msg) noexcept;
    
    // Convert raw price data to Price32nd
    static Price32nd parse_price_32nd(const char* raw_data) noexcept;
};
```

### 3. Feed Handler Pipeline
```cpp
// Main feed handler with quality controls
class TreasuryFeedHandler {
public:
    TreasuryFeedHandler(size_t buffer_size = 8192) noexcept;
    
    // Process incoming raw messages
    size_t process_messages(
        const RawMarketMessage* raw_messages,
        size_t message_count) noexcept;
    
    // Get parsed treasury ticks
    size_t get_parsed_ticks(TreasuryTick* output_buffer, size_t max_count) noexcept;
    
    // Get parsed treasury trades  
    size_t get_parsed_trades(TreasuryTrade* output_buffer, size_t max_count) noexcept;
    
    // Quality control statistics
    struct QualityStats {
        uint64_t total_messages_processed;
        uint64_t invalid_messages;
        uint64_t duplicate_messages;
        uint64_t sequence_gaps;
        HFTTimer::ns_t avg_parse_latency_ns;
        HFTTimer::ns_t max_parse_latency_ns;
    };
    
    QualityStats get_quality_stats() const noexcept;
    void reset_stats() noexcept;
    
private:
    // Internal ring buffers for parsed messages
    TreasuryTickBuffer tick_buffer_;
    TreasuryTradeBuffer trade_buffer_;
    
    // Quality control state
    uint64_t expected_sequence_;
    std::array<uint64_t, 1024> recent_messages_; // Duplicate detection
    size_t recent_messages_idx_;
    
    // Performance tracking
    QualityStats stats_;
    LatencyHistogram parse_latency_hist_;
};
```

### 4. Message Normalization
```cpp
// Handle different exchange message formats
class MessageNormalizer {
public:
    // Convert exchange-specific instrument IDs to TreasuryType
    static TreasuryType normalize_instrument_id(uint32_t exchange_id) noexcept;
    
    // Normalize timestamps to consistent nanosecond precision
    static HFTTimer::timestamp_t normalize_timestamp(
        uint64_t exchange_timestamp, 
        uint32_t exchange_id) noexcept;
    
    // Normalize price formats (handle different decimal places)
    static Price32nd normalize_price(
        const char* raw_price, 
        uint32_t format_type) noexcept;
    
    // Normalize size/quantity (handle different units)
    static uint64_t normalize_size(
        uint64_t raw_size, 
        uint32_t exchange_id) noexcept;
};
```

### 5. Integration with Existing Infrastructure
```cpp
// Feed handler processor with object pool integration
class FeedHandlerProcessor {
public:
    FeedHandlerProcessor() noexcept;
    
    // Process messages using object pools for allocation
    template<typename MessagePool>
    size_t process_with_pools(
        const RawMarketMessage* raw_messages,
        size_t count,
        MessagePool& tick_pool,
        MessagePool& trade_pool) noexcept;
    
    // Batch processing with ring buffer output
    size_t process_to_ring_buffers(
        const RawMarketMessage* raw_messages,
        size_t count,
        TreasuryTickBuffer& tick_buffer,
        TreasuryTradeBuffer& trade_buffer) noexcept;
    
    // Get processing performance metrics
    HFTTimer::ns_t get_avg_processing_latency() const noexcept;
    uint64_t get_messages_per_second() const noexcept;
};
```

## Performance Requirements (HFT Critical)
- **Message parsing:** <200ns per message
- **Batch processing:** >1M messages/second sustained throughput
- **Memory usage:** Zero allocations in hot paths
- **Cache efficiency:** Prefetch next messages during processing
- **Branch prediction:** Optimize for common message types (likely/unlikely)
- **Error handling:** No exceptions, return error codes

## Quality Control Features
- **Sequence number validation:** Detect gaps, out-of-order messages
- **Duplicate detection:** Ring buffer of recent message IDs
- **Checksum validation:** Fast CRC or simple checksum verification
- **Format validation:** Ensure message structure integrity
- **Latency monitoring:** Track parse times, detect performance degradation

## Integration Requirements
- **Treasury data compatibility:** Output TreasuryTick/TreasuryTrade messages
- **Object pool support:** Work with existing ObjectPool templates
- **Ring buffer support:** Output to existing SPSCRingBuffer
- **HFTTimer integration:** Comprehensive latency measurement
- **32nd pricing support:** Use existing Price32nd conversions

## Testing Requirements
Include comprehensive unit tests and benchmarks:
- **Parse accuracy:** Verify correct conversion of raw data
- **Performance tests:** Message parsing latency measurement
- **Throughput tests:** Sustained high-volume processing
- **Quality control tests:** Sequence gaps, duplicates, corruption detection
- **Integration tests:** End-to-end with ring buffers and object pools
- **Stress tests:** Handle malformed messages gracefully

## API Design Patterns
- **Template specializations:** Per-message-type optimizations
- **Constexpr functions:** Compile-time format validation
- **Static asserts:** Validate message sizes and alignments
- **Memory prefetching:** Batch processing optimizations
- **SIMD potential:** Consider ARM64 NEON for batch operations

Generate complete single-header implementation with comprehensive benchmarks, following our proven HFT optimization patterns and integrating seamlessly with existing treasury market data structures.
