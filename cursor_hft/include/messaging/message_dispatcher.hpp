#pragma once

#include <unordered_map>
#include <functional>
#include <type_traits>
#include <memory>
#include <array>
#include "message.hpp"
#include "ring_buffer.hpp"

namespace hft {

// Forward declarations
template<typename T> class MessageHandler;
class MessageDispatcher;

// Message handler interface
template<typename T>
class MessageHandler {
    static_assert(IsMessage<T>, "T must be a message type");
public:
    virtual ~MessageHandler() = default;
    virtual void handle_message(const T& msg) noexcept = 0;
};

// Message dispatcher for routing messages between components
class MessageDispatcher {
public:
    static constexpr size_t MAX_HANDLERS = 32;
    static constexpr size_t DEFAULT_BUFFER_SIZE = 1024;

    MessageDispatcher() = default;
    ~MessageDispatcher() = default;

    // Register a handler for a specific message type
    template<typename T>
    void register_handler(MessageHandler<T>* handler) noexcept {
        static_assert(IsMessage<T>, "T must be a message type");
        handlers_[T::type_id()] = [handler](const void* msg) noexcept {
            handler->handle_message(*static_cast<const T*>(msg));
        };
    }

    // Dispatch a single message
    template<typename T>
    bool dispatch(const T& msg) noexcept {
        static_assert(IsMessage<T>, "T must be a message type");
        auto it = handlers_.find(T::type_id());
        if (it != handlers_.end()) {
            it->second(&msg);
            return true;
        }
        return false;
    }

    // Batch dispatch messages
    template<typename Iterator>
    size_t dispatch_batch(Iterator begin, Iterator end) noexcept {
        size_t count = 0;
        for (auto it = begin; it != end; ++it) {
            if (dispatch(*it)) {
                ++count;
            }
        }
        return count;
    }

private:
    using HandlerFunc = std::function<void(const void*)>;
    std::unordered_map<uint32_t, HandlerFunc> handlers_;
};

// Priority-based message queue
template<typename T, size_t BufferSize = DEFAULT_BUFFER_SIZE>
class PriorityMessageQueue {
    static_assert(IsMessage<T>, "T must be a message type");
public:
    PriorityMessageQueue() = default;

    // Try to enqueue a message
    [[nodiscard]] bool try_enqueue(const T& msg) noexcept {
        return buffers_[static_cast<size_t>(msg.priority())].try_push(msg);
    }

    // Try to dequeue a message (priority order)
    [[nodiscard]] bool try_dequeue(T& msg) noexcept {
        for (auto& buffer : buffers_) {
            if (buffer.try_pop(msg)) {
                return true;
            }
        }
        return false;
    }

    // Batch enqueue messages
    template<typename Iterator>
    size_t try_enqueue_batch(Iterator begin, Iterator end) noexcept {
        size_t total = 0;
        for (auto it = begin; it != end; ++it) {
            if (try_enqueue(*it)) {
                ++total;
            }
        }
        return total;
    }

    // Batch dequeue messages
    template<typename Iterator>
    size_t try_dequeue_batch(Iterator begin, Iterator end) noexcept {
        size_t total = 0;
        for (auto it = begin; it != end; ++it) {
            if (!try_dequeue(*it)) {
                break;
            }
            ++total;
        }
        return total;
    }

private:
    // One buffer per priority level
    std::array<SPSCRingBuffer<T, BufferSize>, 
               static_cast<size_t>(MessagePriority::BACKGROUND) + 1> buffers_;
};

} // namespace hft 