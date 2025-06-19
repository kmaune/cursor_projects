#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <array>
#include <algorithm>
#include <string_view>
#include <atomic>
#include <utility>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using hft::HFTTimer;
using hft::LatencyHistogram;

namespace hft {
namespace market_data {

// ========================= 1. Message Format Definitions =========================

struct alignas(64) RawMarketMessage {
    uint64_t sequence_number;        // Message sequence from exchange
    uint64_t timestamp_exchange_ns;  // Exchange timestamp
    uint32_t message_type;           // Tick, Trade, etc.
    uint32_t instrument_id;          // Maps to TreasuryType
    char raw_data[32];               // Raw price/size data
    uint16_t checksum;               // Message integrity
    uint8_t _pad[6];                 // Pad to 64 bytes (struct is exactly 64 bytes)
};
static_assert(sizeof(RawMarketMessage) == 64, "RawMarketMessage must be 64 bytes (cache line aligned)");

enum class MessageType : uint32_t {
    Tick = 1,
    Trade = 2,
    Heartbeat = 3,
    Invalid = 0
};

enum class ValidationResult : uint8_t {
    Valid,
    InvalidChecksum,
    InvalidSequence,
    InvalidFormat,
    Duplicate
};

// ========================= 2. Message Normalization =========================

class MessageNormalizer {
public:
    static TreasuryType normalize_instrument_id(uint32_t exchange_id) noexcept {
        switch (exchange_id) {
            case 1: return TreasuryType::Bill_3M;
            case 2: return TreasuryType::Bill_6M;
            case 3: return TreasuryType::Note_2Y;
            case 4: return TreasuryType::Note_5Y;
            case 5: return TreasuryType::Note_10Y;
            case 6: return TreasuryType::Bond_30Y;
            default: return TreasuryType::Bill_3M;
        }
    }
    static HFTTimer::timestamp_t normalize_timestamp(
        uint64_t exchange_timestamp, uint32_t /*exchange_id*/) noexcept {
        // For now, passthrough. Could apply per-exchange offset.
        return exchange_timestamp;
    }
    static Price32nd normalize_price(const char* raw_price, uint32_t /*format_type*/) noexcept {
        double price = 0;
        std::memcpy(&price, raw_price, sizeof(double));
        return Price32nd::from_decimal(price);
    }
    static uint64_t normalize_size(uint64_t raw_size, uint32_t /*exchange_id*/) noexcept {
        return raw_size;
    }
};

// ========================= 3. High-Performance Message Parser =========================

template<typename OutputType>
class MessageParser {
public:
    // Parse raw message into treasury data structure
    static ValidationResult parse_message(
        const RawMarketMessage& raw_msg,
        OutputType& parsed_msg,
        HFTTimer::ns_t& parse_latency_ns) noexcept {
        auto start = HFTTimer::get_cycles();
        if (!validate_checksum(raw_msg)) {
            parse_latency_ns = HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
            return ValidationResult::InvalidChecksum;
        }
        if constexpr (std::is_same_v<OutputType, TreasuryTick>) {
            if (!parse_tick(raw_msg, parsed_msg)) {
                parse_latency_ns = HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
                return ValidationResult::InvalidFormat;
            }
        } else if constexpr (std::is_same_v<OutputType, TreasuryTrade>) {
            if (!parse_trade(raw_msg, parsed_msg)) {
                parse_latency_ns = HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
                return ValidationResult::InvalidFormat;
            }
        } else {
            parse_latency_ns = HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
            return ValidationResult::InvalidFormat;
        }
        parse_latency_ns = HFTTimer::cycles_to_ns(HFTTimer::get_cycles() - start);
        return ValidationResult::Valid;
    }

    // Batch parsing for high throughput
    template<typename InputIterator, typename OutputIterator>
    static size_t parse_batch(
        InputIterator raw_begin, InputIterator raw_end,
        OutputIterator parsed_begin, OutputIterator parsed_end,
        size_t& invalid_count) noexcept {
        size_t count = 0;
        invalid_count = 0;
        auto it = raw_begin;
        auto out = parsed_begin;
        for (; it != raw_end && out != parsed_end; ++it, ++out) {
            HFTTimer::ns_t latency = 0;
            ValidationResult res = parse_message(*it, *out, latency);
            if (res == ValidationResult::Valid) {
                ++count;
            } else {
                ++invalid_count;
            }
            // Prefetch next message for cache efficiency
            if constexpr (sizeof(RawMarketMessage) == 64) {
                if ((it + 1) != raw_end) __builtin_prefetch(&(*(it + 1)), 0, 3);
            }
        }
        return count;
    }

private:
    // Fast checksum validation (simple XOR over all bytes except checksum field)
    static bool validate_checksum(const RawMarketMessage& msg) noexcept {
        uint16_t sum = 0;
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&msg);
        for (size_t i = 0; i < offsetof(RawMarketMessage, checksum); ++i) {
            sum ^= ptr[i];
        }
        return sum == msg.checksum;
    }

    // Parse tick message from raw bytes
    static bool parse_tick(const RawMarketMessage& raw, TreasuryTick& out) noexcept {
        out.instrument_type = MessageNormalizer::normalize_instrument_id(raw.instrument_id);
        out.timestamp_ns = MessageNormalizer::normalize_timestamp(raw.timestamp_exchange_ns, raw.instrument_id);
        out.bid_price = MessageNormalizer::normalize_price(raw.raw_data, 0);
        out.ask_price = MessageNormalizer::normalize_price(raw.raw_data + 8, 0);
        std::memcpy(&out.bid_size, raw.raw_data + 16, sizeof(uint64_t));
        std::memcpy(&out.ask_size, raw.raw_data + 24, sizeof(uint64_t));
        out.bid_yield = YieldCalculator::price_to_yield(
            TreasuryInstrument(out.instrument_type, 0, 0), out.bid_price, 0);
        out.ask_yield = YieldCalculator::price_to_yield(
            TreasuryInstrument(out.instrument_type, 0, 0), out.ask_price, 0);
        return out.is_valid();
    }

    // Parse trade message from raw bytes
    static bool parse_trade(const RawMarketMessage& raw, TreasuryTrade& out) noexcept {
        out.instrument_type = MessageNormalizer::normalize_instrument_id(raw.instrument_id);
        out.timestamp_ns = MessageNormalizer::normalize_timestamp(raw.timestamp_exchange_ns, raw.instrument_id);
        out.trade_price = MessageNormalizer::normalize_price(raw.raw_data, 0);
        std::memcpy(&out.trade_size, raw.raw_data + 8, sizeof(uint64_t));
        out.trade_yield = YieldCalculator::price_to_yield(
            TreasuryInstrument(out.instrument_type, 0, 0), out.trade_price, 0);
        std::memcpy(&out.trade_id, raw.raw_data + 16, 16);
        return true;
    }

    // Convert raw price data to Price32nd
    static Price32nd parse_price_32nd(const char* raw_data) noexcept {
        double price = 0;
        std::memcpy(&price, raw_data, sizeof(double));
        return Price32nd::from_decimal(price);
    }
};

// ========================= 4. Feed Handler Pipeline =========================

class TreasuryFeedHandler {
public:
    TreasuryFeedHandler(size_t buffer_size = 8192) noexcept
        : expected_sequence_(0), recent_messages_{}, recent_messages_idx_(0), stats_{}, parse_latency_hist_() {}

    // Process incoming raw messages
    size_t process_messages(
        const RawMarketMessage* raw_messages,
        size_t message_count) noexcept {
        size_t processed = 0;
        for (size_t i = 0; i < message_count; ++i) {
            const RawMarketMessage& msg = raw_messages[i];
            // Sequence validation
            if (expected_sequence_ && msg.sequence_number != expected_sequence_) {
                ++stats_.sequence_gaps;
                expected_sequence_ = msg.sequence_number;
            }
            expected_sequence_ = msg.sequence_number + 1;
            // Duplicate detection
            if (is_duplicate(msg.sequence_number)) {
                ++stats_.duplicate_messages;
                continue;
            }
            add_recent(msg.sequence_number);
            // Parse and classify
            HFTTimer::ns_t latency = 0;
            ValidationResult res = ValidationResult::InvalidFormat;
            if (msg.message_type == static_cast<uint32_t>(MessageType::Tick)) {
                TreasuryTick tick;
                res = MessageParser<TreasuryTick>::parse_message(msg, tick, latency);
                if (res == ValidationResult::Valid) {
                    (void)tick_buffer_.try_push(tick);
                }
            } else if (msg.message_type == static_cast<uint32_t>(MessageType::Trade)) {
                TreasuryTrade trade;
                res = MessageParser<TreasuryTrade>::parse_message(msg, trade, latency);
                if (res == ValidationResult::Valid) {
                    (void)trade_buffer_.try_push(trade);
                }
            } else {
                res = ValidationResult::InvalidFormat;
            }
            parse_latency_hist_.record_latency(latency);
            ++stats_.total_messages_processed;
            if (res != ValidationResult::Valid) ++stats_.invalid_messages;
            // Prefetch next message
            if (i + 1 < message_count) __builtin_prefetch(&raw_messages[i + 1], 0, 3);
            ++processed;
        }
        return processed;
    }

    // Get parsed treasury ticks
    size_t get_parsed_ticks(TreasuryTick* output_buffer, size_t max_count) noexcept {
        return tick_buffer_.try_pop_batch(output_buffer, output_buffer + max_count);
    }

    // Get parsed treasury trades
    size_t get_parsed_trades(TreasuryTrade* output_buffer, size_t max_count) noexcept {
        return trade_buffer_.try_pop_batch(output_buffer, output_buffer + max_count);
    }

    // Quality control statistics
    struct QualityStats {
        uint64_t total_messages_processed = 0;
        uint64_t invalid_messages = 0;
        uint64_t duplicate_messages = 0;
        uint64_t sequence_gaps = 0;
        HFTTimer::ns_t avg_parse_latency_ns = 0;
        HFTTimer::ns_t max_parse_latency_ns = 0;
    };

    QualityStats get_quality_stats() const noexcept {
        auto stats = stats_;
        auto hist = parse_latency_hist_.get_stats();
        stats.avg_parse_latency_ns = static_cast<HFTTimer::ns_t>(hist.mean_latency);
        stats.max_parse_latency_ns = hist.max_latency;
        return stats;
    }
    void reset_stats() noexcept {
        stats_ = {};
        parse_latency_hist_.reset();
    }

private:
    TreasuryTickBuffer tick_buffer_;
    TreasuryTradeBuffer trade_buffer_;
    uint64_t expected_sequence_;
    std::array<uint64_t, 1024> recent_messages_;
    size_t recent_messages_idx_;
    QualityStats stats_;
    LatencyHistogram parse_latency_hist_;

    bool is_duplicate(uint64_t seq) const noexcept {
        return std::find(recent_messages_.begin(), recent_messages_.end(), seq) != recent_messages_.end();
    }
    void add_recent(uint64_t seq) noexcept {
        recent_messages_[recent_messages_idx_++ % recent_messages_.size()] = seq;
    }
};

// ========================= 5. Feed Handler Processor =========================

class FeedHandlerProcessor {
public:
    FeedHandlerProcessor() noexcept : avg_latency_ns_(0), messages_processed_(0) {}

    // Process messages using object pools for allocation
    template<typename MessagePool>
    size_t process_with_pools(
        const RawMarketMessage* raw_messages,
        size_t count,
        MessagePool& tick_pool,
        MessagePool& trade_pool) noexcept {
        size_t processed = 0;
        for (size_t i = 0; i < count; ++i) {
            const RawMarketMessage& msg = raw_messages[i];
            HFTTimer::ns_t latency = 0;
            if (msg.message_type == static_cast<uint32_t>(MessageType::Tick)) {
                auto* tick = tick_pool.acquire();
                if (tick) {
                    if (MessageParser<TreasuryTick>::parse_message(msg, *tick, latency) == ValidationResult::Valid) {
                        ++processed;
                    } else {
                        tick_pool.release(tick);
                    }
                }
            } else if (msg.message_type == static_cast<uint32_t>(MessageType::Trade)) {
                auto* trade = trade_pool.acquire();
                if (trade) {
                    if (MessageParser<TreasuryTrade>::parse_message(msg, *trade, latency) == ValidationResult::Valid) {
                        ++processed;
                    } else {
                        trade_pool.release(trade);
                    }
                }
            }
            avg_latency_ns_ += latency;
            ++messages_processed_;
            if (i + 1 < count) __builtin_prefetch(&raw_messages[i + 1], 0, 3);
        }
        return processed;
    }

    // Batch processing with ring buffer output
    size_t process_to_ring_buffers(
        const RawMarketMessage* raw_messages,
        size_t count,
        TreasuryTickBuffer& tick_buffer,
        TreasuryTradeBuffer& trade_buffer) noexcept {
        size_t processed = 0;
        for (size_t i = 0; i < count; ++i) {
            const RawMarketMessage& msg = raw_messages[i];
            HFTTimer::ns_t latency = 0;
            if (msg.message_type == static_cast<uint32_t>(MessageType::Tick)) {
                TreasuryTick tick;
                if (MessageParser<TreasuryTick>::parse_message(msg, tick, latency) == ValidationResult::Valid) {
                    (void)tick_buffer.try_push(tick);
                    ++processed;
                }
            } else if (msg.message_type == static_cast<uint32_t>(MessageType::Trade)) {
                TreasuryTrade trade;
                if (MessageParser<TreasuryTrade>::parse_message(msg, trade, latency) == ValidationResult::Valid) {
                    (void)trade_buffer.try_push(trade);
                    ++processed;
                }
            }
            avg_latency_ns_ += latency;
            ++messages_processed_;
            if (i + 1 < count) __builtin_prefetch(&raw_messages[i + 1], 0, 3);
        }
        return processed;
    }

    HFTTimer::ns_t get_avg_processing_latency() const noexcept {
        return messages_processed_ ? avg_latency_ns_ / messages_processed_ : 0;
    }
    uint64_t get_messages_per_second() const noexcept {
        // This is a stub; real implementation would use wall clock
        return 0;
    }

private:
    HFTTimer::ns_t avg_latency_ns_;
    uint64_t messages_processed_;
};

} // namespace market_data
} // namespace hft 