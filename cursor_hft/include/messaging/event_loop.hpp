#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <array>
#include "message_dispatcher.hpp"
#include "hft/timing/hft_timer.hpp"

namespace hft {

// Event loop for deterministic message processing
class EventLoop {
public:
    static constexpr size_t MAX_QUEUES = 8;
    static constexpr size_t BATCH_SIZE = 32;
    static constexpr uint64_t DEFAULT_TIMEOUT_NS = 1000; // 1 microsecond

    EventLoop() : 
        running_(false),
        timeout_ns_(DEFAULT_TIMEOUT_NS) {}

    ~EventLoop() {
        stop();
    }

    // Start the event loop in a separate thread
    void start() noexcept {
        if (!running_.exchange(true)) {
            thread_ = std::thread(&EventLoop::run, this);
        }
    }

    // Stop the event loop
    void stop() noexcept {
        if (running_.exchange(false)) {
            if (thread_.joinable()) {
                thread_.join();
            }
        }
    }

    // Set processing timeout
    void set_timeout(uint64_t timeout_ns) noexcept {
        timeout_ns_ = timeout_ns;
    }

    // Add a message queue to the event loop
    template<typename T>
    void add_queue(PriorityMessageQueue<T>* queue) noexcept {
        static_assert(IsMessage<T>, "T must be a message type");
        if (queue_count_ < MAX_QUEUES) {
            queues_[queue_count_++] = [queue](MessageDispatcher& dispatcher) {
                process_queue(*queue, dispatcher);
            };
        }
    }

    // Set the message dispatcher
    void set_dispatcher(MessageDispatcher* dispatcher) noexcept {
        dispatcher_ = dispatcher;
    }

private:
    // Process a single queue
    template<typename T>
    static void process_queue(PriorityMessageQueue<T>& queue, 
                            MessageDispatcher& dispatcher) noexcept {
        std::array<T, BATCH_SIZE> batch;
        size_t count = queue.try_dequeue_batch(batch.begin(), batch.end());
        if (count > 0) {
            dispatcher.dispatch_batch(batch.begin(), batch.begin() + count);
        }
    }

    // Main event loop
    void run() noexcept {
        while (running_.load(std::memory_order_relaxed)) {
            const uint64_t start_time = HFTTimer::get_timestamp_ns();
            
            // Process all queues
            for (size_t i = 0; i < queue_count_; ++i) {
                if (dispatcher_) {
                    queues_[i](*dispatcher_);
                }
            }

            // Calculate sleep time to maintain timing
            const uint64_t elapsed = HFTTimer::get_timestamp_ns() - start_time;
            if (elapsed < timeout_ns_) {
                std::this_thread::sleep_for(
                    std::chrono::nanoseconds(timeout_ns_ - elapsed));
            }
        }
    }

    std::atomic<bool> running_;
    std::thread thread_;
    uint64_t timeout_ns_;
    size_t queue_count_ = 0;
    std::array<std::function<void(MessageDispatcher&)>, MAX_QUEUES> queues_;
    MessageDispatcher* dispatcher_ = nullptr;
};

} // namespace hft 