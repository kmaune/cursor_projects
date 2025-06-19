# Primary Dealer Venue Simulation - High-Performance Treasury Market Connectivity

Create a high-performance primary dealer venue simulation for our HFT treasury trading system as a single header file.

## Requirements
- **Single header implementation:** `include/hft/market_data/venue_simulation.hpp`
- **Primary dealer focus:** Realistic treasury market venue behavior
- **Order lifecycle simulation:** Submit → Acknowledge → Fill/Reject with proper timing
- **ARM64 optimization:** 64-byte cache alignment, sub-microsecond performance
- **Zero allocation:** All processing in pre-allocated buffers during hot paths
- **Integration:** Seamless with existing feed handler and treasury data structures
- **Performance monitoring:** Built-in latency measurement with HFTTimer
- **Future-proof design:** Foundation for generic venue framework evolution

## Core Components Needed

### 1. Order and Venue Message Definitions
```cpp
// Treasury-specific order types
enum class OrderType : uint8_t {
    Limit,              // Standard limit order
    YieldLimit,         // Yield-based limit order
    Market,             // Market order (rare in treasury)
    Cancel,             // Cancel existing order
    Invalid = 0xFF
};

enum class OrderSide : uint8_t { Buy, Sell };

enum class OrderStatus : uint8_t {
    New,                // Just created
    Submitted,          // Sent to venue
    Acknowledged,       // Venue confirmed receipt
    PartiallyFilled,    // Partial execution
    Filled,             // Fully executed
    Rejected,           // Venue rejected
    Cancelled,          // Successfully cancelled
    Expired             // Time-based expiration
};

// Cache-aligned order structure
struct alignas(64) TreasuryOrder {
    uint64_t order_id;                          // Unique order identifier
    uint64_t client_order_id;                   // Client-side tracking ID
    HFTTimer::timestamp_t timestamp_created_ns; // Order creation time
    HFTTimer::timestamp_t timestamp_venue_ns;   // Last venue interaction
    TreasuryType instrument_type;               // Which treasury instrument
    OrderType order_type;                       // Order type
    OrderSide side;                             // Buy/Sell
    OrderStatus status;                         // Current order status
    Price32nd limit_price;                      // Limit price in 32nds
    double yield_limit;                         // For yield-based orders (4 decimals)
    uint64_t quantity;                          // Order size in millions
    uint64_t filled_quantity;                   // Already filled amount
    uint64_t remaining_quantity;                // Unfilled amount
    char venue_order_id[16];                    // Venue-assigned ID
    uint8_t _pad[8];                            // Pad to 64 bytes
};
static_assert(sizeof(TreasuryOrder) == 64, "TreasuryOrder must be 64 bytes");

// Venue response messages
struct alignas(64) VenueResponse {
    uint64_t order_id;                          // References TreasuryOrder
    HFTTimer::timestamp_t timestamp_venue_ns;   // Venue timestamp
    OrderStatus new_status;                     // Status update
    uint64_t fill_quantity;                     // If filled, how much
    Price32nd fill_price;                       // If filled, at what price
    char venue_order_id[16];                    // Venue tracking ID
    char reject_reason[24];                     // If rejected, why
    uint8_t _pad[7];                            // Pad to 64 bytes
};
static_assert(sizeof(VenueResponse) == 64, "VenueResponse must be 64 bytes");
```

### 2. Realistic Latency and Fill Modeling
```cpp
// Venue latency characteristics
struct VenueLatencyModel {
    HFTTimer::ns_t base_latency_ns;        // Base round-trip time (e.g., 50μs)
    HFTTimer::ns_t jitter_std_dev_ns;      // Gaussian jitter (e.g., 10μs)
    HFTTimer::ns_t queue_delay_ns;         // Additional delay under load
    double queue_probability;               // Probability of queue delay (0.0-1.0)
    
    // Calculate realistic venue latency
    HFTTimer::ns_t calculate_latency() const noexcept;
};

// Fill probability and pricing model
class TreasuryFillModel {
public:
    // Calculate fill probability based on order aggressiveness
    static double calculate_fill_probability(
        const TreasuryOrder& order,
        const TreasuryTick& current_market) noexcept;
    
    // Determine fill price (price improvement modeling)
    static Price32nd calculate_fill_price(
        const TreasuryOrder& order,
        const TreasuryTick& current_market) noexcept;
    
    // Model partial fill behavior
    static uint64_t calculate_fill_quantity(
        const TreasuryOrder& order,
        double fill_probability) noexcept;
};
```

### 3. Primary Dealer Venue Simulator
```cpp
class PrimaryDealerVenue {
public:
    PrimaryDealerVenue(
        std::string_view venue_name = "PrimaryDealer1",
        VenueLatencyModel latency_model = default_latency_model()) noexcept;
    
    // Submit new order to venue
    bool submit_order(const TreasuryOrder& order) noexcept;
    
    // Cancel existing order
    bool cancel_order(uint64_t order_id) noexcept;
    
    // Process market data updates (affects fill decisions)
    void process_market_update(const TreasuryTick& tick) noexcept;
    
    // Get venue responses (fills, acks, rejects)
    size_t get_venue_responses(VenueResponse* output_buffer, size_t max_count) noexcept;
    
    // Update venue state (process pending orders, generate fills)
    void update_venue_state() noexcept;
    
    // Venue statistics and monitoring
    struct VenueStats {
        uint64_t orders_submitted;
        uint64_t orders_acknowledged;
        uint64_t orders_filled;
        uint64_t orders_rejected;
        HFTTimer::ns_t avg_ack_latency_ns;
        HFTTimer::ns_t avg_fill_latency_ns;
        double fill_rate;
    };
    
    VenueStats get_venue_stats() const noexcept;
    void reset_stats() noexcept;

private:
    // Internal order management
    using OrderMap = std::array<TreasuryOrder, 4096>; // Fixed-size order storage
    using ResponseBuffer = SPSCRingBuffer<VenueResponse, 8192>;
    
    OrderMap active_orders_;
    ResponseBuffer response_buffer_;
    VenueLatencyModel latency_model_;
    TreasuryTick current_market_;
    VenueStats stats_;
    
    // Internal order processing
    void process_pending_acknowledgments() noexcept;
    void process_pending_fills() noexcept;
    void generate_venue_response(const TreasuryOrder& order, OrderStatus new_status) noexcept;
};
```

### 4. Order Management Integration
```cpp
// Order router that connects strategies to venues
class TreasuryOrderRouter {
public:
    TreasuryOrderRouter() noexcept;
    
    // Register venue for order routing
    void add_venue(std::unique_ptr<PrimaryDealerVenue> venue) noexcept;
    
    // Route order to appropriate venue
    bool route_order(const TreasuryOrder& order) noexcept;
    
    // Process all venue responses
    size_t process_venue_responses() noexcept;
    
    // Get filled orders for strategy processing
    size_t get_fills(VenueResponse* output_buffer, size_t max_count) noexcept;
    
    // Market data integration (feed to all venues)
    void process_market_data(const TreasuryTick& tick) noexcept;
    
    // Performance monitoring
    HFTTimer::ns_t get_avg_routing_latency() const noexcept;

private:
    std::vector<std::unique_ptr<PrimaryDealerVenue>> venues_;
    SPSCRingBuffer<VenueResponse, 16384> consolidated_responses_;
    LatencyHistogram routing_latency_hist_;
};
```

### 5. Strategy Integration Framework
```cpp
// Simple market-making strategy for testing venue simulation
class TestMarketMakingStrategy {
public:
    TestMarketMakingStrategy(TreasuryOrderRouter& router) noexcept;
    
    // Process incoming market data and generate orders
    void process_market_tick(const TreasuryTick& tick) noexcept;
    
    // Handle order fills and update positions
    void process_fill(const VenueResponse& fill) noexcept;
    
    // Get strategy performance metrics
    struct StrategyStats {
        uint64_t orders_sent;
        uint64_t fills_received;
        double pnl_unrealized;
        int64_t net_position;
    };
    
    StrategyStats get_strategy_stats() const noexcept;

private:
    TreasuryOrderRouter& router_;
    std::atomic<uint64_t> next_order_id_;
    StrategyStats stats_;
    
    // Simple spread calculation
    std::pair<Price32nd, Price32nd> calculate_quotes(const TreasuryTick& tick) noexcept;
};
```

## Performance Requirements (HFT Critical)
- **Order submission:** <500ns end-to-end latency
- **Fill processing:** <300ns per fill
- **Market data processing:** <100ns per tick update
- **Memory usage:** Zero allocations in hot paths
- **Latency simulation:** Configurable 10-100μs realistic ranges
- **Throughput:** Handle >10K orders/second per venue

## Integration Requirements
- **Feed handler compatibility:** Consume TreasuryTick messages seamlessly
- **Object pool support:** Use existing ObjectPool templates for order management
- **Ring buffer support:** Output to existing SPSCRingBuffer infrastructure
- **HFTTimer integration:** Comprehensive latency measurement and venue timing
- **Treasury data compatibility:** Full 32nd pricing and yield calculation support

## Venue Simulation Features
- **Realistic acknowledgment timing:** Model venue processing delays
- **Fill probability modeling:** Based on order aggressiveness and market conditions
- **Price improvement simulation:** Realistic partial fill behavior
- **Queue dynamics:** Model venue capacity constraints and priority
- **Rejection handling:** Proper error codes and reject reasons

## Testing Requirements
Include comprehensive unit tests and benchmarks:
- **Order lifecycle tests:** Submit → Ack → Fill validation
- **Latency accuracy tests:** Verify venue timing simulation
- **Fill modeling tests:** Price improvement and quantity validation
- **Integration tests:** End-to-end with feed handler and strategies
- **Performance tests:** Order processing latency and throughput
- **Stress tests:** High-volume order flow handling

## API Design Patterns
- **Template specializations:** Per-instrument-type optimizations
- **Constexpr functions:** Compile-time venue configuration
- **Static asserts:** Validate message sizes and alignments
- **Memory prefetching:** Batch order processing optimizations
- **Future-proof interfaces:** Design for easy VenueInterface abstraction

## Treasury Market Specifics
- **Primary dealer behavior:** Institutional-grade order handling
- **32nd pricing precision:** Full integration with Price32nd system
- **Yield-based orders:** Support yield limit orders with proper conversion
- **Settlement considerations:** T+1 settlement modeling
- **Size conventions:** Million-dollar face value units

Generate complete single-header implementation with comprehensive benchmarks and tests, following our proven HFT optimization patterns and integrating seamlessly with existing treasury market data infrastructure. This should provide the foundation for multi-venue evolution while delivering immediate value for strategy development.
