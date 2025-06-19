Fix the following critical HFT performance issues in our ObjectPool implementation:

**Issue 1: Object Memory Alignment**
Current code: `alignas(CACHE_LINE_SIZE) std::byte storage_[PoolSize][sizeof(T)];`
Problem: This aligns the array, not individual objects within it
Fix: Change to `alignas(CACHE_LINE_SIZE) T storage_[PoolSize];` to ensure each object is cache-aligned

**Issue 2: Metadata Memory Waste**
Current code: Each metadata field gets its own 64-byte cache line
Problem: 192 bytes used for 24 bytes of data (8x memory waste)
Fix: Group all metadata into a single cache-aligned struct:
```cpp
struct alignas(CACHE_LINE_SIZE) Metadata {
    size_type free_top_ = PoolSize;
    size_type allocated_ = 0;
    hft::HFTTimer::ns_t last_alloc_ns_ = 0;
    // padding to fill cache line if needed
};
```

**Issue 3: Hot Path Performance Overhead**
Current code: Timing measurement in every acquire() call
Problem: Adds 15-30ns overhead to every allocation
Fix: Make timing optional with compile-time flag:
- Add template parameter: `bool EnableTiming = false`
- Wrap timing code in `if constexpr (EnableTiming)`
- Provide separate `acquire_timed()` method for when timing is needed

**Additional Requirements:**
- Maintain all existing functionality and API
- Keep comprehensive unit tests working
- Add benchmark comparing timed vs non-timed performance
- Ensure the object storage properly aligns objects to cache boundaries
- Validate memory layout with static_asserts or runtime checks

**Performance Targets:**
- Non-timed acquire(): <10ns
- Timed acquire(): <25ns  
- Memory efficiency: Minimal padding/waste
- Cache behavior: Objects aligned to 64-byte boundaries

Fix these issues while preserving the excellent HFT design patterns already implemented.
