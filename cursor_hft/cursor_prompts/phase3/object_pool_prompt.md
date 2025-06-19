Create a high-performance object pool for our HFT system as a single header file.

Requirements:
- Single header implementation (include/hft/memory/object_pool.hpp)
- Template-based ObjectPool<T, PoolSize> for any object type
- Fixed-size pool with deterministic memory layout
- ARM64 cache-line optimization (64-byte alignment)
- Zero allocation after initialization - pre-allocate all objects
- Single-threaded design for maximum performance (no atomics)
- Integration with our existing HFTTimer for allocation timing
- Stack-based free list for O(1) allocation/deallocation

API Requirements:
- ObjectPool<T, PoolSize> template class
- acquire() -> T* (get object from pool)
- release(T* obj) -> void (return object to pool)
- size() -> size_t (current allocated count)
- capacity() -> size_t (total pool capacity)
- available() -> size_t (free objects remaining)
- reset() -> void (return all objects to pool)

Performance Features:
- Use static_assert for type validation (trivially destructible recommended)
- Prefetch next allocation slot during acquire()
- Ensure PoolSize is reasonable (static_assert < reasonable limit)
- Cache-line align pool metadata separately from object storage
- Add memory prefetching for batch allocation patterns

Include:
- Comprehensive unit tests with edge cases
- Performance benchmarks measuring allocation/deallocation latency
- Integration with HFTTimer for timing validation
- CMake integration
- Memory usage validation tests

Keep it simple - just the object pool, no complex memory managers or multiple allocator types.
