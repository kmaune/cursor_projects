# Header Organization & Performance Patterns

## Single-Header Component Pattern (Proven Effective)
- Complete component functionality in one header file
- Template-based implementation for compile-time optimization
- Minimal external dependencies
- Cache-aligned data structures

## Cache Alignment Requirements (ARM64)
```cpp
// All performance-critical structures must be cache-aligned
struct alignas(64) PerformanceCriticalData {
    // 64-byte alignment for ARM64 cache lines
};
```

## Object Pool Integration Pattern
```cpp
// All components must integrate with object pools
template<typename T>
class ComponentWithPools {
    ObjectPool<T>& pool_;
    // Zero allocation guarantee in hot paths
};
```

## Ring Buffer Messaging Pattern  
```cpp
// Inter-component communication via established ring buffers
SPSCRingBuffer<MessageType>& ring_buffer_;
// 29.7ns proven latency for push/pop operations
```

## Template Optimization Patterns
- Compile-time polymorphism over runtime polymorphism
- Template specialization for performance-critical paths
- SFINAE for type safety without runtime overhead
- Constexpr for compile-time calculations

## Treasury Market Data Integration
- 32nd fractional pricing support mandatory
- Yield calculation integration required
- Price conversion compatibility essential
- Market message format consistency
