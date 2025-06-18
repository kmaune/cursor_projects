Create a simple, high-performance SPSC ring buffer for our HFT system as a single header file.

Requirements:
- Single header implementation (include/hft/messaging/spsc_ring_buffer.hpp)
- Template-based for type safety with any message type
- ARM64 cache-line optimization (64-byte alignment)  
- Zero allocation in hot paths
- Integration with our existing HFTTimer for timestamping
- Lock-free atomic operations with proper memory ordering
- Use std::atomic with explicit memory_order parameters
- Ensure capacity is power-of-2 for efficient masking
- Add static_asserts for trivially_copyable types
- Include memory prefetching hints for batch operations

API Requirements:
- SPSCRingBuffer<T, Size> template class
- try_push(const T& item) -> bool
- try_pop(T& item) -> bool
- try_push_batch(Iterator begin, Iterator end) -> size_t
- try_pop_batch(Iterator begin, Iterator end) -> size_t
- Basic capacity/size/empty/full methods

Include:
- Comprehensive unit tests
- Performance benchmarks measuring:
  - Single push/pop latency (target: <50ns)
  - Batch operation throughput
  - Validation under producer/consumer thread stress
- Integration with our HFTTimer system
- CMake integration

Keep it simple - just the ring buffer, no complex dispatchers or event loops.
