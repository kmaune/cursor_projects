#pragma once

#include "message.hpp"
#include <cstdint>
#include <string_view>

namespace hft {

// Market data message types
struct MarketDataMessage : public Message<MarketDataMessage> {
    enum class Type : uint8_t {
        TICK,
        ORDER,
        TRADE
    };

    Type type;
    std::string_view symbol;
    uint64_t price;
    uint32_t quantity;
    uint64_t sequence;
};

// Trading message types
struct OrderMessage : public Message<OrderMessage> {
    enum class Type : uint8_t {
        NEW,
        CANCEL,
        MODIFY
    };

    Type type;
    std::string_view order_id;
    std::string_view symbol;
    uint64_t price;
    uint32_t quantity;
    bool is_buy;
};

struct TradeMessage : public Message<TradeMessage> {
    std::string_view trade_id;
    std::string_view symbol;
    uint64_t price;
    uint32_t quantity;
    bool is_buy;
    uint64_t timestamp;
};

// System message types
struct HeartbeatMessage : public Message<HeartbeatMessage> {
    uint64_t sequence;
    uint64_t timestamp;
};

struct StatusMessage : public Message<StatusMessage> {
    enum class Type : uint8_t {
        INFO,
        WARNING,
        ERROR
    };

    Type type;
    std::string_view component;
    std::string_view message;
    uint64_t timestamp;
};

} // namespace hft 