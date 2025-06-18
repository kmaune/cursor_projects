#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <array>
#include <memory>
#include <arm_neon.h>
#include "../timing/hft_timer.hpp"

namespace hft {

/**
 * @brief High-performance Single Producer Single Consumer (SPSC) ring buffer
 * 
 * This implementation provides a lock-free, zero-allocation ring buffer optimized
 * for HFT systems with sub-microsecond latency requirements. Features include:
 * - Cache-line aligned data structures
 * - ARM64 optimized memory operations
 * - Power-of-2 capacity for efficient masking
 * - Batch operations with prefetching
 * - Integration with HFTTimer for latency tracking
 * 
 * @tparam T Message type (must be trivially copyable)
 * @tparam Size Buffer capacity (must be power of 2)
 */
template<typename T, size_t Size>
class alignas(HFTTimer::CACHE_LINE_SIZE) SPSCRingBuffer {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
    static_assert(Size > 0, "Size must be greater than 0");

public:
    using value_type = T;
    using size_type = size_t;
    using timestamp_t = HFTTimer::timestamp_t;

    // Constructor
    SPSCRingBuffer() noexcept : head_(0), tail_(0) {
        // Prefetch the entire buffer to warm cache
        for (size_t i = 0; i < Size; i += HFTTimer::CACHE_LINE_SIZE) {
            __builtin_prefetch(&buffer_[i], 1, 3);
        }
    }

    // Prevent copying
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    /**
     * @brief Try to push a single item into the buffer
     * @param item Item to push
     * @return true if successful, false if buffer is full
     */
    [[nodiscard]] bool try_push(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & (Size - 1);
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        // Prefetch the next write location
        __builtin_prefetch(&buffer_[next_head], 1, 3);
        
        // Copy the item
        std::memcpy(&buffer_[head], &item, sizeof(T));
        
        // Update head with release semantics
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to pop a single item from the buffer
     * @param item Reference to store popped item
     * @return true if successful, false if buffer is empty
     */
    [[nodiscard]] bool try_pop(T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        // Prefetch the next read location
        __builtin_prefetch(&buffer_[(tail + 1) & (Size - 1)], 0, 3);
        
        // Copy the item
        std::memcpy(&item, &buffer_[tail], sizeof(T));
        
        // Update tail with release semantics
        tail_.store((tail + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to push multiple items into the buffer
     * @param begin Iterator to first item
     * @param end Iterator past last item
     * @return Number of items successfully pushed
     */
    template<typename Iterator>
    [[nodiscard]] size_t try_push_batch(Iterator begin, Iterator end) noexcept {
        size_t count = 0;
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_acquire);
        const size_t available = Size - ((head - tail) & (Size - 1)) - 1;
        
        if (available == 0) {
            return 0;
        }

        const size_t batch_size = std::min(available, static_cast<size_t>(std::distance(begin, end)));
        
        // Prefetch the entire batch
        for (size_t i = 0; i < batch_size; i += HFTTimer::CACHE_LINE_SIZE) {
            __builtin_prefetch(&buffer_[(head + i) & (Size - 1)], 1, 3);
        }

        // Copy items
        for (size_t i = 0; i < batch_size; ++i) {
            std::memcpy(&buffer_[(head + i) & (Size - 1)], &(*begin++), sizeof(T));
        }

        // Update head with release semantics
        head_.store((head + batch_size) & (Size - 1), std::memory_order_release);
        return batch_size;
    }

    /**
     * @brief Try to pop multiple items from the buffer
     * @param begin Iterator to first destination
     * @param end Iterator past last destination
     * @return Number of items successfully popped
     */
    template<typename Iterator>
    [[nodiscard]] size_t try_pop_batch(Iterator begin, Iterator end) noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t available = (head - tail) & (Size - 1);
        
        if (available == 0) {
            return 0;
        }

        const size_t batch_size = std::min(available, static_cast<size_t>(std::distance(begin, end)));
        
        // Prefetch the entire batch
        for (size_t i = 0; i < batch_size; i += HFTTimer::CACHE_LINE_SIZE) {
            __builtin_prefetch(&buffer_[(tail + i) & (Size - 1)], 0, 3);
        }

        // Copy items
        for (size_t i = 0; i < batch_size; ++i) {
            std::memcpy(&(*begin++), &buffer_[(tail + i) & (Size - 1)], sizeof(T));
        }

        // Update tail with release semantics
        tail_.store((tail + batch_size) & (Size - 1), std::memory_order_release);
        return batch_size;
    }

    /**
     * @brief Get current size of the buffer
     * @return Number of items in the buffer
     */
    [[nodiscard]] size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & (Size - 1);
    }

    /**
     * @brief Check if buffer is empty
     * @return true if buffer is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if buffer is full
     * @return true if buffer is full
     */
    [[nodiscard]] bool full() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return ((head + 1) & (Size - 1)) == tail;
    }

    /**
     * @brief Get maximum capacity of the buffer
     * @return Maximum number of items the buffer can hold
     */
    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return Size - 1;  // One slot reserved to distinguish empty from full
    }

private:
    // Cache-aligned buffer storage
    alignas(HFTTimer::CACHE_LINE_SIZE) std::array<T, Size> buffer_;
    
    // Producer position (head)
    alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<size_t> head_;
    
    // Consumer position (tail)
    alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<size_t> tail_;
};

} // namespace hft 