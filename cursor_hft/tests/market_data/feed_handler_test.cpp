#include <gtest/gtest.h>
#include "hft/market_data/feed_handler.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include <random>
#include <cstring>
#include <vector>

using hft::HFTTimer;

using namespace hft::market_data;

namespace {

// Helper: Generate valid checksum for a RawMarketMessage
uint16_t compute_checksum(const RawMarketMessage& msg) {
    uint16_t sum = 0;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&msg);
    for (size_t i = 0; i < offsetof(RawMarketMessage, checksum); ++i) sum ^= ptr[i];
    return sum;
}

// Helper: Create a valid RawMarketMessage (Tick or Trade)
RawMarketMessage make_raw_msg(uint64_t seq, MessageType type, uint32_t instr_id, double price, uint64_t size) {
    RawMarketMessage msg{};
    msg.sequence_number = seq;
    msg.timestamp_exchange_ns = 1000000 + seq;
    msg.message_type = static_cast<uint32_t>(type);
    msg.instrument_id = instr_id;
    double price_val = price;
    if (type == MessageType::Tick) {
        std::memcpy(msg.raw_data, &price_val, sizeof(double));
        std::memcpy(msg.raw_data + 8, &price_val, sizeof(double)); // ask price
        std::memcpy(msg.raw_data + 16, &size, sizeof(uint64_t));   // bid size
        std::memcpy(msg.raw_data + 24, &size, sizeof(uint64_t));   // ask size
    } else if (type == MessageType::Trade) {
        std::memcpy(msg.raw_data, &price_val, sizeof(double));     // trade price
        std::memcpy(msg.raw_data + 8, &size, sizeof(uint64_t));    // trade size
        // Fill trade_id with dummy data (e.g., all 0xAB)
        uint8_t trade_id[16];
        std::memset(trade_id, 0xAB, sizeof(trade_id));
        std::memcpy(msg.raw_data + 16, trade_id, 16);              // trade_id
    }
    msg.checksum = compute_checksum(msg);
    return msg;
}

// Helper: Create a batch of valid messages
std::vector<RawMarketMessage> make_batch(size_t n, MessageType type, uint32_t instr_id) {
    std::vector<RawMarketMessage> batch;
    for (size_t i = 0; i < n; ++i) {
        batch.push_back(make_raw_msg(i + 1, type, instr_id, 100.0 + i, 1000 + i));
    }
    return batch;
}

// Helper: Corrupt checksum
void corrupt_checksum(RawMarketMessage& msg) { msg.checksum ^= 0xFF; }

// Helper: Random valid instrument id
uint32_t random_instr_id() {
    static std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(1, 6);
    return dist(rng);
}

} // namespace

TEST(FeedHandler, StructAlignment) {
    EXPECT_EQ(sizeof(RawMarketMessage), 64u);
    EXPECT_EQ(alignof(RawMarketMessage), 64u);
}

TEST(FeedHandler, ChecksumValidation) {
    auto msg = make_raw_msg(1, MessageType::Tick, 1, 101.5, 1000);
    HFTTimer::ns_t latency = 0;
    TreasuryTick tick;
    EXPECT_EQ(MessageParser<TreasuryTick>::parse_message(msg, tick, latency), ValidationResult::Valid);
    corrupt_checksum(msg);
    EXPECT_EQ(MessageParser<TreasuryTick>::parse_message(msg, tick, latency), ValidationResult::InvalidChecksum);
}

TEST(FeedHandler, TickParsingAccuracy) {
    auto msg = make_raw_msg(2, MessageType::Tick, 2, 102.25, 2000);
    HFTTimer::ns_t latency = 0;
    TreasuryTick tick;
    EXPECT_EQ(MessageParser<TreasuryTick>::parse_message(msg, tick, latency), ValidationResult::Valid);
    EXPECT_EQ(tick.instrument_type, MessageNormalizer::normalize_instrument_id(2));
    EXPECT_GT(tick.bid_price.whole, 0);
    EXPECT_GT(tick.ask_price.whole, 0);
    EXPECT_EQ(tick.bid_size, 2000u);
    EXPECT_EQ(tick.ask_size, 2000u);
}

TEST(FeedHandler, TradeParsingAccuracy) {
    auto msg = make_raw_msg(3, MessageType::Trade, 3, 103.75, 3000);
    HFTTimer::ns_t latency = 0;
    TreasuryTrade trade;
    EXPECT_EQ(MessageParser<TreasuryTrade>::parse_message(msg, trade, latency), ValidationResult::Valid);
    EXPECT_EQ(trade.instrument_type, MessageNormalizer::normalize_instrument_id(3));
    EXPECT_GT(trade.trade_price.whole, 0);
    EXPECT_EQ(trade.trade_size, 3000u);
}

TEST(FeedHandler, BatchParsing) {
    auto batch = make_batch(100, MessageType::Tick, 4);
    std::vector<TreasuryTick> ticks(100);
    size_t invalid = 0;
    size_t parsed = MessageParser<TreasuryTick>::parse_batch(batch.begin(), batch.end(), ticks.begin(), ticks.end(), invalid);
    EXPECT_EQ(parsed, 100u);
    EXPECT_EQ(invalid, 0u);
}

TEST(FeedHandler, FeedHandlerWorkflow) {
    TreasuryFeedHandler handler;
    auto batch = make_batch(10, MessageType::Tick, 5);
    size_t processed = handler.process_messages(batch.data(), batch.size());
    EXPECT_EQ(processed, 10u);
    std::vector<TreasuryTick> out(10);
    size_t got = handler.get_parsed_ticks(out.data(), out.size());
    EXPECT_EQ(got, 10u);
}

TEST(FeedHandler, QualityControlDuplicatesAndSequence) {
    TreasuryFeedHandler handler;
    auto batch = make_batch(5, MessageType::Tick, 1);
    batch[2].sequence_number = batch[1].sequence_number; // duplicate
    batch[4].sequence_number = 100; // sequence gap
    handler.process_messages(batch.data(), batch.size());
    auto stats = handler.get_quality_stats();
    EXPECT_EQ(stats.duplicate_messages, 1u);
    EXPECT_EQ(stats.sequence_gaps, 1u);
}

TEST(FeedHandler, MessageNormalization) {
    EXPECT_EQ(MessageNormalizer::normalize_instrument_id(1), TreasuryType::Bill_3M);
    EXPECT_EQ(MessageNormalizer::normalize_instrument_id(6), TreasuryType::Bond_30Y);
    double price = 99.5;
    char buf[8];
    std::memcpy(buf, &price, sizeof(double));
    auto p32 = MessageNormalizer::normalize_price(buf, 0);
    EXPECT_NEAR(p32.to_decimal(), 99.5, 1e-6);
}

TEST(FeedHandler, ObjectPoolIntegration) {
    TreasuryTickPool tick_pool;
    TreasuryTradePool trade_pool;
    auto msg = make_raw_msg(10, MessageType::Tick, 2, 105.0, 5000);
    HFTTimer::ns_t latency = 0;
    auto* tick = tick_pool.acquire();
    ASSERT_NE(tick, nullptr);
    EXPECT_EQ(MessageParser<TreasuryTick>::parse_message(msg, *tick, latency), ValidationResult::Valid);
    tick_pool.release(tick);
}

TEST(FeedHandler, RingBufferIntegration) {
    TreasuryTickBuffer tick_buffer;
    auto batch = make_batch(8, MessageType::Tick, 3);
    for (const auto& msg : batch) {
        TreasuryTick tick;
        HFTTimer::ns_t latency = 0;
        if (MessageParser<TreasuryTick>::parse_message(msg, tick, latency) == ValidationResult::Valid) {
            tick_buffer.try_push(tick);
        }
    }
    std::vector<TreasuryTick> out(8);
    size_t got = tick_buffer.try_pop_batch(out.begin(), out.end());
    EXPECT_EQ(got, 8u);
}

TEST(FeedHandler, ErrorHandlingEdgeCases) {
    RawMarketMessage msg = make_raw_msg(1, MessageType::Tick, 1, 0.0, 0); // invalid price/size
    HFTTimer::ns_t latency = 0;
    TreasuryTick tick;
    EXPECT_NE(MessageParser<TreasuryTick>::parse_message(msg, tick, latency), ValidationResult::Valid);
} 