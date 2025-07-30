# Component Implementation Prompt Templates

Reusable prompt templates for HFT infrastructure component development. These templates provide consistent starting points for building high-performance, cache-optimized components with zero-allocation design.

**Recommended Agent**: `@claude/agents/hft-component-builder.md`

## Template Categories

### 1. Memory Management Component

**Use Case**: Object pools, custom allocators, memory management systems  
**Customization Parameters**: `[COMPONENT_TYPE]`, `[POOL_SIZE]`, `[ALIGNMENT]`, `[OBJECT_TYPE]`

```
Implement a high-performance [COMPONENT_TYPE] memory management component for HFT trading system with zero-allocation guarantees.

COMPONENT REQUIREMENTS:
- Component type: [COMPONENT_TYPE] (object pool/allocator/memory mapper)
- Pool size: [POOL_SIZE] objects with [ALIGNMENT]-byte alignment
- Object type: [OBJECT_TYPE] with size optimization
- Performance target: <25ns allocation/deallocation operations

PERFORMANCE SPECIFICATIONS:
- Allocation latency: <15ns per operation (target <10ns)
- Deallocation latency: <10ns per operation
- Memory alignment: [ALIGNMENT]-byte boundaries for ARM64 cache optimization
- Zero allocation: No dynamic allocation after initialization
- Cache efficiency: Optimize for 64-byte ARM64 cache lines

IMPLEMENTATION REQUIREMENTS:
- Single-header template implementation in `include/hft/memory/[component_name].hpp`
- Template-based design for compile-time optimization
- Cache-aligned data structures with proper padding
- Lock-free design with atomic operations where needed
- Integration with existing HFTTimer for performance measurement

ARCHITECTURE PATTERN:
```cpp
namespace hft::memory {

template<typename T, size_t PoolSize, bool EnableMetrics = false>
class [COMPONENT_TYPE] {
    static_assert(PoolSize > 0 && PoolSize <= 65536, "Reasonable pool size");
    
public:
    // Core operations (must be <25ns)
    T* acquire() noexcept;
    void release(T* obj) noexcept;
    
    // Performance monitoring
    template<bool Enabled = EnableMetrics>
    std::enable_if_t<Enabled, HFTTimer::ns_t> get_avg_latency() const noexcept;
    
private:
    // Cache-aligned storage
    alignas(64) T storage_[PoolSize];
    alignas(64) std::array<T*, PoolSize> free_list_;
    alignas(64) size_t free_top_;
};

} // namespace hft::memory
```

INTEGRATION REQUIREMENTS:
- HFTTimer integration for latency measurement
- Object pool compatibility with existing patterns
- Ring buffer messaging for component communication
- CMakeLists.txt integration with proper linking

TESTING REQUIREMENTS:
- Unit tests for functional correctness
- Performance benchmarks validating <25ns target
- Integration tests with existing infrastructure
- Memory allocation verification (zero in hot paths)
- Stress tests under high allocation rates

Provide complete component implementation with comprehensive testing and performance validation.
```

### 2. Messaging Component

**Use Case**: Ring buffers, message queues, communication systems  
**Customization Parameters**: `[QUEUE_TYPE]`, `[CAPACITY]`, `[MESSAGE_TYPE]`, `[CONCURRENCY_MODEL]`

```
Implement a high-performance [QUEUE_TYPE] messaging component with [CONCURRENCY_MODEL] concurrency for HFT inter-component communication.

COMPONENT REQUIREMENTS:
- Queue type: [QUEUE_TYPE] (SPSC/MPSC/MPMC ring buffer)
- Capacity: [CAPACITY] messages (power-of-2 for efficient modulo)
- Message type: [MESSAGE_TYPE] with optimal size
- Concurrency: [CONCURRENCY_MODEL] with lock-free design

PERFORMANCE SPECIFICATIONS:
- Operation latency: <50ns per push/pop operation
- Throughput: >10M operations/sec sustained
- Memory ordering: Explicit atomic operations with proper synchronization
- Cache efficiency: ARM64 cache-line optimization
- Zero allocation: Pre-allocated ring buffer design

IMPLEMENTATION REQUIREMENTS:
- Single-header template implementation in `include/hft/messaging/[component_name].hpp`
- Lock-free algorithms with atomic operations
- Power-of-2 capacity for efficient masking
- Cache-aligned producer/consumer separation
- Memory prefetching for predictable access patterns

ARCHITECTURE PATTERN:
```cpp
namespace hft::messaging {

template<typename T, size_t Capacity>
class [QUEUE_TYPE] {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
public:
    // Core operations (must be <50ns)
    bool try_push(const T& item) noexcept;
    bool try_pop(T& item) noexcept;
    
    // Batch operations for throughput
    template<typename Iterator>
    size_t try_push_batch(Iterator begin, Iterator end) noexcept;
    
private:
    // Cache line separation to avoid false sharing
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::array<T, Capacity> buffer_;
};

} // namespace hft::messaging
```

CONCURRENCY DESIGN:
- [CONCURRENCY_MODEL] specific atomic operations and memory ordering
- Producer/consumer cache line separation for false sharing prevention
- Memory barriers appropriate for ARM64 architecture
- Wait-free or lock-free guarantees as appropriate

INTEGRATION REQUIREMENTS:
- Treasury data structure compatibility
- HFTTimer integration for latency measurement
- Object pool integration for message allocation
- Ring buffer chaining for complex workflows

TESTING REQUIREMENTS:
- Single-threaded performance benchmarks
- Multi-threaded stress tests for [CONCURRENCY_MODEL]
- Integration tests with existing messaging infrastructure
- Throughput validation under realistic load
- Memory ordering correctness verification

Provide complete messaging component with concurrency validation and performance benchmarks.
```

### 3. Data Structure Component

**Use Case**: Hash tables, trees, specialized data structures  
**Customization Parameters**: `[STRUCTURE_TYPE]`, `[KEY_TYPE]`, `[VALUE_TYPE]`, `[CAPACITY]`

```
Implement a cache-optimized [STRUCTURE_TYPE] data structure for HFT applications with <100ns operation latency.

COMPONENT REQUIREMENTS:
- Structure type: [STRUCTURE_TYPE] (hash table/B+ tree/priority queue)
- Key type: [KEY_TYPE] with efficient hashing/comparison
- Value type: [VALUE_TYPE] with optimal storage layout
- Capacity: [CAPACITY] elements with cache-friendly organization

PERFORMANCE SPECIFICATIONS:
- Operation latency: <100ns per lookup/insert/delete
- Cache efficiency: Optimize for ARM64 L1 cache (64 KiB)
- Memory layout: Cache-line aligned with prefetching
- Concurrent access: Lock-free where possible, minimal contention

IMPLEMENTATION REQUIREMENTS:
- Single-header template implementation in `include/hft/structures/[component_name].hpp`
- Cache-aware memory layout with 64-byte alignment
- Template specialization for common types
- Branch prediction optimization with likely/unlikely hints
- Memory prefetching for predictable access patterns

ARCHITECTURE PATTERN:
```cpp
namespace hft::structures {

template<typename Key, typename Value, size_t Capacity>
class [STRUCTURE_TYPE] {
    static_assert(Capacity > 0, "Non-zero capacity required");
    
public:
    // Core operations (must be <100ns)
    bool find(const Key& key, Value& value) noexcept;
    bool insert(const Key& key, const Value& value) noexcept;
    bool remove(const Key& key) noexcept;
    
    // Cache-friendly iteration
    template<typename Func>
    void for_each(Func&& func) const noexcept;
    
private:
    // Cache-optimized storage layout
    struct alignas(64) CacheLineBucket {
        // Pack multiple entries per cache line
        static constexpr size_t ENTRIES_PER_LINE = 
            (64 - sizeof(metadata)) / sizeof(Entry);
        std::array<Entry, ENTRIES_PER_LINE> entries;
        uint32_t metadata;  // Occupancy/version info
    };
    
    alignas(64) std::array<CacheLineBucket, bucket_count> buckets_;
};

} // namespace hft::structures
```

OPTIMIZATION TECHNIQUES:
- Hash function optimization for [KEY_TYPE]
- Cache-line packing with multiple entries per line
- Branch-free algorithms where possible
- Memory prefetching for sequential access patterns
- Template specialization for performance-critical types

INTEGRATION REQUIREMENTS:
- Compatible with treasury data types (Price32nd, TreasuryTick)
- HFTTimer integration for operation timing
- Object pool integration for node allocation
- Exception-free design with error codes

TESTING REQUIREMENTS:
- Performance benchmarks for all operations
- Cache behavior analysis and optimization
- Stress tests with realistic data patterns
- Memory layout validation and cache miss measurement
- Integration tests with HFT data types

Provide complete data structure implementation with cache optimization analysis and performance validation.
```

### 4. Timing Component

**Use Case**: High-precision timers, latency measurement, performance monitoring  
**Customization Parameters**: `[TIMER_TYPE]`, `[PRECISION_TARGET]`, `[MEASUREMENT_SCOPE]`

```
Implement a high-precision [TIMER_TYPE] timing component for HFT latency measurement with [PRECISION_TARGET] precision.

COMPONENT REQUIREMENTS:
- Timer type: [TIMER_TYPE] (cycle counter/histogram/monitor)
- Precision target: [PRECISION_TARGET] nanosecond accuracy
- Measurement scope: [MEASUREMENT_SCOPE] (single operation/system-wide/component-specific)
- ARM64 integration: Use cntvct_el0 cycle counter for maximum precision

PERFORMANCE SPECIFICATIONS:
- Timing overhead: <20ns per measurement
- Resolution: Sub-nanosecond with cycle counter accuracy
- Calibration: Automatic cycles-to-nanoseconds conversion
- Statistical analysis: Comprehensive latency distribution analysis

IMPLEMENTATION REQUIREMENTS:
- Single-header template implementation in `include/hft/timing/[component_name].hpp`
- ARM64 cycle counter integration with calibration
- Lock-free histogram collection for multi-threaded use
- Statistical analysis (min/max/mean/percentiles/stddev)
- Memory-efficient storage with cache optimization

ARCHITECTURE PATTERN:
```cpp
namespace hft::timing {

class [TIMER_TYPE] {
public:
    using ns_t = uint64_t;
    using cycle_t = uint64_t;
    
    // High-precision timing (must be <20ns overhead)
    static cycle_t get_cycles() noexcept;
    static ns_t cycles_to_ns(cycle_t cycles) noexcept;
    static ns_t get_timestamp_ns() noexcept;
    
    // Scoped timing for automatic measurement
    class ScopedTimer {
        cycle_t start_cycles_;
        LatencyHistogram& histogram_;
    public:
        ScopedTimer(LatencyHistogram& hist) noexcept;
        ~ScopedTimer() noexcept;
    };
    
private:
    static std::atomic<double> cycles_per_ns_;
    static void calibrate() noexcept;
};

} // namespace hft::timing
```

ARM64 INTEGRATION:
- Direct cntvct_el0 cycle counter access
- Automatic calibration using system clock
- Drift correction for long-running measurements
- Architecture-specific optimization

STATISTICAL FRAMEWORK:
- Lock-free histogram with atomic operations
- Percentile calculation (50th, 95th, 99th, 99.9th)
- Standard deviation and variance calculation
- Outlier detection and analysis

INTEGRATION REQUIREMENTS:
- Compatible with existing HFTTimer patterns
- Object pool integration for timer objects
- Ring buffer integration for timing data collection
- Treasury component timing validation

TESTING REQUIREMENTS:
- Calibration accuracy validation
- Timing overhead measurement
- Statistical analysis verification
- Multi-threaded histogram testing
- Integration with existing timing infrastructure

Provide complete timing component with ARM64 optimization and comprehensive statistical analysis.
```

### 5. Lock-Free Algorithm Component

**Use Case**: Atomic operations, wait-free algorithms, concurrent data structures  
**Customization Parameters**: `[ALGORITHM_TYPE]`, `[SYNCHRONIZATION_MODEL]`, `[MEMORY_ORDERING]`

```
Implement a lock-free [ALGORITHM_TYPE] component using [SYNCHRONIZATION_MODEL] with [MEMORY_ORDERING] memory ordering guarantees.

COMPONENT REQUIREMENTS:
- Algorithm type: [ALGORITHM_TYPE] (queue/stack/hash table/counter)
- Synchronization: [SYNCHRONIZATION_MODEL] (wait-free/lock-free/obstruction-free)
- Memory ordering: [MEMORY_ORDERING] (acquire-release/sequential consistency)
- ARM64 optimization: Architecture-specific atomic operations

PERFORMANCE SPECIFICATIONS:
- Operation latency: Algorithm-specific targets (typically <100ns)
- Contention handling: Graceful performance under high contention
- Memory ordering: Minimal synchronization overhead
- ABA prevention: Proper hazard pointer or epoch-based management

IMPLEMENTATION REQUIREMENTS:
- Single-header template implementation in `include/hft/concurrent/[component_name].hpp`
- C++20 atomic operations with explicit memory ordering
- ARM64-specific optimizations and memory barriers
- Comprehensive correctness proofs in comments
- Performance validation under contention

ARCHITECTURE PATTERN:
```cpp
namespace hft::concurrent {

template<typename T, size_t Capacity>
class LockFree[ALGORITHM_TYPE] {
public:
    // Core operations with [SYNCHRONIZATION_MODEL] guarantees
    bool try_operation(const T& item) noexcept;
    bool try_dequeue(T& item) noexcept;
    
    // Non-blocking status queries
    bool empty() const noexcept;
    size_t approximate_size() const noexcept;
    
private:
    // Atomic state with [MEMORY_ORDERING] semantics
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    
    // Memory ordering helpers
    void acquire_fence() noexcept { 
        std::atomic_thread_fence(std::memory_order_acquire); 
    }
    void release_fence() noexcept { 
        std::atomic_thread_fence(std::memory_order_release); 
    }
};

} // namespace hft::concurrent
```

SYNCHRONIZATION DESIGN:
- [SYNCHRONIZATION_MODEL] implementation with correctness proofs
- [MEMORY_ORDERING] memory ordering for ARM64 architecture
- ABA problem prevention with appropriate techniques
- Hazard pointer management for memory reclamation

CORRECTNESS VALIDATION:
- Formal verification of lock-free properties
- Linearizability analysis and proof
- Progress guarantee verification ([SYNCHRONIZATION_MODEL])
- Memory safety validation under all scenarios

INTEGRATION REQUIREMENTS:
- HFT data type compatibility
- Performance measurement integration
- Object pool integration for node management
- Error handling without exceptions

TESTING REQUIREMENTS:
- Single-threaded correctness validation
- Multi-threaded stress testing with high contention
- Performance benchmarks under various load patterns
- Memory ordering correctness verification
- ABA prevention validation

Provide complete lock-free implementation with correctness proofs and comprehensive testing.
```

### 6. ARM64 Optimization Component

**Use Case**: Platform-specific optimizations, SIMD operations, hardware features  
**Customization Parameters**: `[OPTIMIZATION_TYPE]`, `[SIMD_USAGE]`, `[FEATURE_SET]`

```
Implement ARM64-optimized [OPTIMIZATION_TYPE] component leveraging [FEATURE_SET] hardware features with [SIMD_USAGE] vectorization.

COMPONENT REQUIREMENTS:
- Optimization type: [OPTIMIZATION_TYPE] (batch processing/computation/data transformation)
- SIMD usage: [SIMD_USAGE] (NEON vectorization/scalar optimization)
- Feature set: [FEATURE_SET] (cache prefetching/branch prediction/memory ordering)
- ARM64 specifics: Architecture-specific instruction optimization

PERFORMANCE SPECIFICATIONS:
- Throughput improvement: Target >2x performance over generic implementation
- Cache efficiency: Optimize for ARM64 cache hierarchy (64 KiB L1)
- SIMD utilization: Leverage NEON for appropriate batch operations
- Branch optimization: Minimize branch mispredictions in hot paths

IMPLEMENTATION REQUIREMENTS:
- Single-header template implementation in `include/hft/arm64/[component_name].hpp`
- ARM64 intrinsics and inline assembly where beneficial
- NEON SIMD operations for vectorizable computations
- Cache-friendly memory access patterns
- Compiler optimization hints and pragmas

ARCHITECTURE PATTERN:
```cpp
namespace hft::arm64 {

template<typename T, size_t BatchSize = 16>
class ARM64[OPTIMIZATION_TYPE] {
    static_assert(BatchSize % 4 == 0, "Batch size must be NEON-friendly");
    
public:
    // Optimized batch operations
    void process_batch(const T* input, T* output, size_t count) noexcept;
    
    // Single-item operations with optimization
    T process_single(const T& input) noexcept;
    
    // SIMD-optimized operations
    void process_neon_batch(const float32x4_t* input, float32x4_t* output, size_t count) noexcept;
    
private:
    // ARM64-specific optimizations
    void prefetch_data(const void* addr) noexcept {
        __builtin_prefetch(addr, 0, 3);  // Read, high temporal locality
    }
    
    // Branch optimization hints
    bool likely_condition(bool condition) noexcept {
        return __builtin_expect(condition, 1);
    }
};

} // namespace hft::arm64
```

ARM64 OPTIMIZATION TECHNIQUES:
- NEON SIMD instructions for [SIMD_USAGE] operations
- Cache prefetching with __builtin_prefetch
- Branch prediction hints with __builtin_expect
- Memory alignment for optimal cache line usage
- ARM64-specific inline assembly for critical sections

SIMD IMPLEMENTATION:
- NEON intrinsics for vectorized operations
- Batch size optimization for SIMD efficiency
- Fallback scalar implementation for edge cases
- Data layout optimization for SIMD access patterns

INTEGRATION REQUIREMENTS:
- Treasury data type compatibility (Price32nd, yield calculations)
- HFT infrastructure integration (timing, memory, messaging)
- Performance measurement with cycle-accurate timing
- Graceful fallback for non-ARM64 platforms

TESTING REQUIREMENTS:
- Performance benchmarks comparing ARM64 vs generic implementation
- SIMD correctness validation with comprehensive test vectors
- Cache behavior analysis and optimization verification
- Integration testing with HFT data processing pipelines
- Portability testing with fallback implementations

Provide complete ARM64-optimized implementation with performance analysis and SIMD validation.
```

## Template Usage Guidelines

### Component Selection Criteria
- **Performance requirements**: Choose template based on latency targets
- **Integration needs**: Consider existing infrastructure compatibility
- **Complexity level**: Match template to implementation complexity
- **Platform specifics**: ARM64 optimization vs portable implementation

### Customization Best Practices
- **Replace all [PARAMETER] placeholders** with specific requirements
- **Adjust performance targets** based on component criticality
- **Specify integration patterns** with existing infrastructure
- **Include comprehensive testing** for all functionality

### Validation Requirements
- **Performance benchmarking** against established targets
- **Integration testing** with existing HFT infrastructure
- **Memory behavior validation** (zero allocation, cache efficiency)
- **Multi-threaded testing** where applicable

These templates provide proven patterns for HFT infrastructure development while maintaining flexibility for specific requirements and optimization targets.
