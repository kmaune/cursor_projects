#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <new>
#include <utility>
#include <cassert>
#include <cstring>
#include <string_view>
#include "hft/timing/hft_timer.hpp"

namespace hft {

/**
 * @brief High-performance fixed-size object pool for HFT systems
 *
 * - Zero allocations after initialization
 * - 64-byte cache-line alignment for all metadata and objects
 * - Stack-based free list for O(1) allocation/deallocation
 * - Single-threaded, no atomics or locks
 * - Optional integration with HFTTimer for allocation timing
 * - Prefetching for next allocation slot
 * - Static asserts for type and size validation
 */
template <typename T, std::size_t PoolSize, bool EnableTiming = false>
class ObjectPool {
    static_assert(PoolSize > 0, "PoolSize must be > 0");
    static_assert(PoolSize <= 65536, "PoolSize unreasonably large for HFT stack pool");
    static_assert(std::is_trivially_destructible_v<T>, "T should be trivially destructible for HFT object pool");
    static_assert(alignof(T) <= 64, "T alignment must not exceed 64 bytes");

public:
    static constexpr std::size_t CACHE_LINE_SIZE = 64;
    using value_type = T;
    using size_type = std::size_t;

    // Metadata grouped into a single cache-line-aligned struct
    struct alignas(CACHE_LINE_SIZE) Metadata {
        size_type free_top_ = PoolSize;
        size_type allocated_ = 0;
        hft::HFTTimer::ns_t last_alloc_ns_ = 0;
        // Padding to fill cache line if needed
        char _pad[CACHE_LINE_SIZE - sizeof(size_type)*2 - sizeof(hft::HFTTimer::ns_t)] = {};
    };

    ObjectPool() noexcept {
        // Pre-allocate all objects and initialize free list
        for (size_type i = 0; i < PoolSize; ++i) {
            free_list_[i] = &storage_[i];
        }
        meta_.free_top_ = PoolSize;
        meta_.allocated_ = 0;
        meta_.last_alloc_ns_ = 0;
        // Validate alignment at runtime
        assert(reinterpret_cast<uintptr_t>(&storage_[0]) % CACHE_LINE_SIZE == 0 && "Object storage not cache-line aligned");
        assert(reinterpret_cast<uintptr_t>(&meta_) % CACHE_LINE_SIZE == 0 && "Metadata not cache-line aligned");
    }

    // No copy or move
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /**
     * @brief Acquire an object from the pool (optionally timed)
     * @return Pointer to T, or nullptr if pool exhausted
     */
    T* acquire() noexcept {
        if constexpr (EnableTiming) {
            auto start = hft::HFTTimer::get_cycles();
            T* obj = acquire_impl();
            meta_.last_alloc_ns_ = hft::HFTTimer::cycles_to_ns(hft::HFTTimer::get_cycles() - start);
            return obj;
        } else {
            return acquire_impl();
        }
    }

    /**
     * @brief Acquire an object from the pool and always time the allocation
     * @return Pointer to T, or nullptr if pool exhausted
     */
    T* acquire_timed() noexcept {
        auto start = hft::HFTTimer::get_cycles();
        T* obj = acquire_impl();
        meta_.last_alloc_ns_ = hft::HFTTimer::cycles_to_ns(hft::HFTTimer::get_cycles() - start);
        return obj;
    }

    /**
     * @brief Release an object back to the pool
     * @param obj Pointer to object previously acquired
     */
    void release(T* obj) noexcept {
        assert(obj >= &storage_[0] && obj < &storage_[PoolSize]);
        assert(meta_.allocated_ > 0);
        free_list_[meta_.free_top_++] = obj;
        --meta_.allocated_;
    }

    /**
     * @brief Number of currently allocated objects
     */
    size_type size() const noexcept { return meta_.allocated_; }

    /**
     * @brief Pool capacity (total number of objects)
     */
    constexpr size_type capacity() const noexcept { return PoolSize; }

    /**
     * @brief Number of available (free) objects
     */
    size_type available() const noexcept { return meta_.free_top_; }

    /**
     * @brief Reset pool to initial state (all objects free)
     */
    void reset() noexcept {
        for (size_type i = 0; i < PoolSize; ++i) {
            free_list_[i] = &storage_[i];
        }
        meta_.free_top_ = PoolSize;
        meta_.allocated_ = 0;
    }

    /**
     * @brief Get last allocation latency in nanoseconds
     */
    hft::HFTTimer::ns_t last_alloc_ns() const noexcept { return meta_.last_alloc_ns_; }

    /**
     * @brief Validate memory usage and alignment
     */
    bool validate_memory() const noexcept {
        // Check each object is cache-line aligned
        for (size_type i = 0; i < PoolSize; ++i) {
            if (reinterpret_cast<uintptr_t>(&storage_[i]) % CACHE_LINE_SIZE != 0) return false;
        }
        // Check metadata alignment
        if (reinterpret_cast<uintptr_t>(&meta_) % CACHE_LINE_SIZE != 0) return false;
        return true;
    }

    // Static asserts for layout
    static_assert(alignof(Metadata) == CACHE_LINE_SIZE, "Metadata struct not cache-line aligned");
    static_assert(sizeof(Metadata) == CACHE_LINE_SIZE, "Metadata struct must fill one cache line");
    static_assert(alignof(T) <= CACHE_LINE_SIZE, "Object alignment must not exceed cache line");

private:
    // 64-byte aligned storage for objects (each object is aligned)
    alignas(CACHE_LINE_SIZE) T storage_[PoolSize];
    // 64-byte aligned free list (stack of pointers)
    alignas(CACHE_LINE_SIZE) T* free_list_[PoolSize];
    // 64-byte aligned metadata (all fields grouped)
    alignas(CACHE_LINE_SIZE) Metadata meta_;

    // Internal acquire implementation (no timing)
    T* acquire_impl() noexcept {
        if (meta_.free_top_ == 0) {
            return nullptr;
        }
        // Prefetch next slot for batch allocation patterns
        if (meta_.free_top_ > 1) {
            __builtin_prefetch(free_list_[meta_.free_top_ - 2], 1, 1);
        }
        T* obj = free_list_[--meta_.free_top_];
        ++meta_.allocated_;
        return obj;
    }
};

} // namespace hft 