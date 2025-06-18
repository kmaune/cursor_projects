#pragma once

#include <cstdint>
#include <type_traits>
#include "hft/timing/hft_timer.hpp"

namespace hft {

// Message priority levels for deterministic processing
enum class MessagePriority : uint8_t {
    CRITICAL = 0,    // Market data, order updates
    HIGH = 1,        // Trading signals
    NORMAL = 2,      // Regular updates
    LOW = 3,         // Status updates
    BACKGROUND = 4   // Non-critical operations
};

// Base message class with timing and type safety
template<typename Derived>
class Message {
public:
    using MessageType = Derived;
    
    // Compile-time type checking
    static constexpr bool is_message = std::is_base_of_v<Message<Derived>, Derived>;
    
    // Constructor with automatic timestamping
    constexpr Message() noexcept : 
        timestamp_(HFTTimer::get_timestamp_ns()),
        priority_(MessagePriority::NORMAL) {}
    
    // Get message timestamp
    [[nodiscard]] constexpr uint64_t timestamp() const noexcept { return timestamp_; }
    
    // Set message priority
    constexpr void set_priority(MessagePriority priority) noexcept { priority_ = priority; }
    
    // Get message priority
    [[nodiscard]] constexpr MessagePriority priority() const noexcept { return priority_; }
    
    // Get message type ID at compile time
    [[nodiscard]] static constexpr uint32_t type_id() noexcept {
        return type_id_impl<Derived>();
    }

private:
    // Compile-time type ID generation
    template<typename T>
    static constexpr uint32_t type_id_impl() noexcept {
        return static_cast<uint32_t>(std::hash<const char*>{}(__PRETTY_FUNCTION__));
    }

    uint64_t timestamp_;  // Nanosecond precision timestamp
    MessagePriority priority_;  // Message priority for deterministic processing
} __attribute__((aligned(64)));  // Cache line alignment

// Type traits for message validation
template<typename T>
concept IsMessage = std::is_base_of_v<Message<T>, T>;

} // namespace hft 