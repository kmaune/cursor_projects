#include "hft/market_data/feed_handler.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"
#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <cstring>

using namespace hft::market_data;
using hft::HFTTimer;

namespace {

RawMarketMessage make_raw_msg(uint64_t seq, MessageType type, uint32_t instr_id, double price, uint64_t size) {
    RawMarketMessage msg{};
    msg.sequence_number = seq;
    msg.timestamp_exchange_ns = 1000000 + seq;
    msg.message_type = static_cast<uint32_t>(type);
    msg.instrument_id = instr_id;
    std::memcpy(msg.raw_data, &price, sizeof(double));
    std::memcpy(msg.raw_data + 8, &price, sizeof(double));
    std::memcpy(msg.raw_data + 16, &size, sizeof(uint64_t));
    std::memcpy(msg.raw_data + 24, &size, sizeof(uint64_t));
    // Compute checksum
    uint16_t sum = 0;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&msg);
    for (size_t i = 0; i < offsetof(RawMarketMessage, checksum); ++i) sum ^= ptr[i];
    msg.checksum = sum;
    return msg;
}

std::vector<RawMarketMessage> make_batch(size_t n, MessageType type, uint32_t instr_id) {
    std::vector<RawMarketMessage> batch;
    for (size_t i = 0; i < n; ++i) {
        batch.push_back(make_raw_msg(i + 1, type, instr_id, 100.0 + i, 1000 + i));
    }
    return batch;
}

} // namespace

static void BM_FeedHandler_SingleMessageLatency(benchmark::State& state) {
    RawMarketMessage msg = make_raw_msg(1, MessageType::Tick, 1, 101.5, 1000);
    TreasuryTick tick;
    
    for (auto _ : state) {
        HFTTimer::ns_t latency = 0;
        MessageParser<TreasuryTick>::parse_message(msg, tick, latency);
        benchmark::DoNotOptimize(tick);
    }
    
    state.SetLabel("Single message parsing latency");
}
BENCHMARK(BM_FeedHandler_SingleMessageLatency);

static void BM_FeedHandler_BatchParsingThroughput(benchmark::State& state) {
    constexpr size_t batch_size = 10000;
    auto batch = make_batch(batch_size, MessageType::Tick, 2);
    std::vector<TreasuryTick> ticks(batch_size);
    
    for (auto _ : state) {
        size_t invalid = 0;
        size_t parsed = MessageParser<TreasuryTick>::parse_batch(
            batch.begin(), batch.end(), ticks.begin(), ticks.end(), invalid);
        benchmark::DoNotOptimize(parsed);
        benchmark::DoNotOptimize(invalid);
    }
    
    state.SetItemsProcessed(batch_size * state.iterations());
    state.SetLabel("Batch parsing throughput");
}
BENCHMARK(BM_FeedHandler_BatchParsingThroughput);

static void BM_FeedHandler_EndToEndThroughput(benchmark::State& state) {
    constexpr size_t batch_size = 10000;
    auto batch = make_batch(batch_size, MessageType::Tick, 2);
    TreasuryFeedHandler handler;
    
    for (auto _ : state) {
        size_t processed = handler.process_messages(batch.data(), batch.size());
        benchmark::DoNotOptimize(processed);
    }
    
    state.SetItemsProcessed(batch_size * state.iterations());
    state.SetLabel("Feed handler end-to-end throughput");
}
BENCHMARK(BM_FeedHandler_EndToEndThroughput);

static void BM_FeedHandler_ObjectPoolPerformance(benchmark::State& state) {
    TreasuryTickPool tick_pool;
    constexpr size_t pool_ops = 1000;
    
    for (auto _ : state) {
        for (size_t i = 0; i < pool_ops; ++i) {
            auto* t = tick_pool.acquire();
            if (t) tick_pool.release(t);
        }
    }
    
    state.SetItemsProcessed(pool_ops * state.iterations());
    state.SetLabel("Object pool acquire/release");
}
BENCHMARK(BM_FeedHandler_ObjectPoolPerformance);

static void BM_FeedHandler_ChecksumValidation(benchmark::State& state) {
    constexpr size_t batch_size = 10000;
    auto batch = make_batch(batch_size, MessageType::Tick, 2);
    
    for (auto _ : state) {
        uint16_t cksum = 0;
        for (size_t i = 0; i < batch_size; ++i) {
            cksum ^= batch[i].checksum;
        }
        benchmark::DoNotOptimize(cksum);
    }
    
    state.SetItemsProcessed(batch_size * state.iterations());
    state.SetLabel("Checksum validation");
}
BENCHMARK(BM_FeedHandler_ChecksumValidation);

static void BM_FeedHandler_MessageNormalization(benchmark::State& state) {
    constexpr size_t batch_size = 10000;
    auto batch = make_batch(batch_size, MessageType::Tick, 2);
    
    for (auto _ : state) {
        for (size_t i = 0; i < batch_size; ++i) {
            auto t = MessageNormalizer::normalize_instrument_id(batch[i].instrument_id);
            benchmark::DoNotOptimize(t);
        }
    }
    
    state.SetItemsProcessed(batch_size * state.iterations());
    state.SetLabel("Message normalization");
}
BENCHMARK(BM_FeedHandler_MessageNormalization);

BENCHMARK_MAIN(); 
