#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <array>

namespace hft {

// Lock-free ring buffer with configurable capacity
template<typename T, size_t Capacity>
class RingBuffer {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    static_assert(Capacity > 0 && ((Capacity & (Capacity - 1)) == 0), 
                 "Capacity must be a power of 2");
    
public:
    using value_type = T;
    static constexpr size_t capacity = Capacity;
    static constexpr size_t mask = Capacity - 1;

    RingBuffer() noexcept : 
        head_(0), 
        tail_(0) {
        // Ensure cache line alignment
        static_assert(alignof(RingBuffer) >= 64, "RingBuffer must be cache-line aligned");
    }

    // Check if buffer is empty
    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }

    // Check if buffer is full
    [[nodiscard]] bool full() const noexcept {
        return size() == Capacity;
    }

    // Get current size
    [[nodiscard]] size_t size() const noexcept {
        return head_.load(std::memory_order_acquire) - 
               tail_.load(std::memory_order_acquire);
    }

protected:
    // Align data to cache line to prevent false sharing
    alignas(64) std::array<T, Capacity> data_;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

// Single Producer Single Consumer Ring Buffer
template<typename T, size_t Capacity>
class SPSCRingBuffer : public RingBuffer<T, Capacity> {
public:
    // Try to push a single item
    [[nodiscard]] bool try_push(const T& item) noexcept {
        const size_t head = this->head_.load(std::memory_order_relaxed);
        const size_t next_head = head + 1;
        
        if (next_head - this->tail_.load(std::memory_order_acquire) > this->capacity) {
            return false;
        }

        this->data_[head & this->mask] = item;
        this->head_.store(next_head, std::memory_order_release);
        return true;
    }

    // Try to pop a single item
    [[nodiscard]] bool try_pop(T& item) noexcept {
        const size_t tail = this->tail_.load(std::memory_order_relaxed);
        
        if (tail == this->head_.load(std::memory_order_acquire)) {
            return false;
        }

        item = this->data_[tail & this->mask];
        this->tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Batch push operation
    template<typename Iterator>
    [[nodiscard]] size_t try_push_batch(Iterator begin, Iterator end) noexcept {
        const size_t head = this->head_.load(std::memory_order_relaxed);
        const size_t tail = this->tail_.load(std::memory_order_acquire);
        const size_t available = this->capacity - (head - tail);
        const size_t count = std::min(available, static_cast<size_t>(std::distance(begin, end)));
        
        for (size_t i = 0; i < count; ++i) {
            this->data_[(head + i) & this->mask] = *begin++;
        }
        
        this->head_.store(head + count, std::memory_order_release);
        return count;
    }

    // Batch pop operation
    template<typename Iterator>
    [[nodiscard]] size_t try_pop_batch(Iterator begin, Iterator end) noexcept {
        const size_t tail = this->tail_.load(std::memory_order_relaxed);
        const size_t head = this->head_.load(std::memory_order_acquire);
        const size_t count = std::min(head - tail, 
                                    static_cast<size_t>(std::distance(begin, end)));
        
        for (size_t i = 0; i < count; ++i) {
            *begin++ = this->data_[(tail + i) & this->mask];
        }
        
        this->tail_.store(tail + count, std::memory_order_release);
        return count;
    }
};

// Multi Producer Single Consumer Ring Buffer
template<typename T, size_t Capacity>
class MPSCRingBuffer : public RingBuffer<T, Capacity> {
public:
    // Try to push a single item
    [[nodiscard]] bool try_push(const T& item) noexcept {
        size_t head = this->head_.load(std::memory_order_relaxed);
        
        while (true) {
            const size_t tail = this->tail_.load(std::memory_order_acquire);
            const size_t next_head = head + 1;
            
            if (next_head - tail > this->capacity) {
                return false;
            }

            if (this->head_.compare_exchange_weak(head, next_head,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
                this->data_[head & this->mask] = item;
                return true;
            }
        }
    }

    // Try to pop a single item (same as SPSC)
    [[nodiscard]] bool try_pop(T& item) noexcept {
        const size_t tail = this->tail_.load(std::memory_order_relaxed);
        
        if (tail == this->head_.load(std::memory_order_acquire)) {
            return false;
        }

        item = this->data_[tail & this->mask];
        this->tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Batch push operation with CAS
    template<typename Iterator>
    [[nodiscard]] size_t try_push_batch(Iterator begin, Iterator end) noexcept {
        const size_t count = std::distance(begin, end);
        size_t head = this->head_.load(std::memory_order_relaxed);
        
        while (true) {
            const size_t tail = this->tail_.load(std::memory_order_acquire);
            const size_t available = this->capacity - (head - tail);
            const size_t push_count = std::min(available, static_cast<size_t>(count));
            
            if (push_count == 0) {
                return 0;
            }

            if (this->head_.compare_exchange_weak(head, head + push_count,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
                for (size_t i = 0; i < push_count; ++i) {
                    this->data_[(head + i) & this->mask] = *begin++;
                }
                return push_count;
            }
        }
    }

    // Batch pop operation (same as SPSC)
    template<typename Iterator>
    [[nodiscard]] size_t try_pop_batch(Iterator begin, Iterator end) noexcept {
        const size_t tail = this->tail_.load(std::memory_order_relaxed);
        const size_t head = this->head_.load(std::memory_order_acquire);
        const size_t count = std::min(head - tail, 
                                    static_cast<size_t>(std::distance(begin, end)));
        
        for (size_t i = 0; i < count; ++i) {
            *begin++ = this->data_[(tail + i) & this->mask];
        }
        
        this->tail_.store(tail + count, std::memory_order_release);
        return count;
    }
};

} // namespace hft 