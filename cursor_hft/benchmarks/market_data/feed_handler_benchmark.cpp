#include "hft/market_data/feed_handler.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"
#include <vector>
#include <iostream>
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

int main() {
    constexpr size_t N = 1000000;
    std::cout << "Feed Handler Benchmark (" << N << " messages)\n";

    // Single message parsing latency
    RawMarketMessage msg = make_raw_msg(1, MessageType::Tick, 1, 101.5, 1000);
    TreasuryTick tick;
    HFTTimer::ns_t min_latency = UINT64_MAX, max_latency = 0, sum_latency = 0;
    for (int i = 0; i < 1000; ++i) {
        HFTTimer::cycle_t start = HFTTimer::get_cycles();
        HFTTimer::ns_t latency = 0;
        MessageParser<TreasuryTick>::parse_message(msg, tick, latency);
        HFTTimer::cycle_t end = HFTTimer::get_cycles();
        auto ns = HFTTimer::cycles_to_ns(end - start);
        min_latency = std::min(min_latency, ns);
        max_latency = std::max(max_latency, ns);
        sum_latency += ns;
    }
    std::cout << "Single message parsing latency: min " << min_latency << " ns, max " << max_latency << " ns, avg " << (sum_latency / 1000) << " ns\n";

    // Batch parsing throughput
    auto batch = make_batch(N, MessageType::Tick, 2);
    std::vector<TreasuryTick> ticks(N);
    size_t invalid = 0;
    HFTTimer::cycle_t batch_start = HFTTimer::get_cycles();
    size_t parsed = MessageParser<TreasuryTick>::parse_batch(batch.begin(), batch.end(), ticks.begin(), ticks.end(), invalid);
    HFTTimer::cycle_t batch_end = HFTTimer::get_cycles();
    double batch_ns = HFTTimer::cycles_to_ns(batch_end - batch_start);
    double throughput = (double)parsed / (batch_ns / 1e9);
    std::cout << "Batch parsing throughput: " << throughput << " messages/sec\n";

    // Feed handler end-to-end processing
    TreasuryFeedHandler handler;
    batch_start = HFTTimer::get_cycles();
    size_t processed = handler.process_messages(batch.data(), batch.size());
    batch_end = HFTTimer::get_cycles();
    batch_ns = HFTTimer::cycles_to_ns(batch_end - batch_start);
    throughput = (double)processed / (batch_ns / 1e9);
    std::cout << "Feed handler end-to-end throughput: " << throughput << " messages/sec\n";

    // Object pool integration performance
    TreasuryTickPool tick_pool;
    HFTTimer::cycle_t pool_start = HFTTimer::get_cycles();
    for (size_t i = 0; i < N; ++i) {
        auto* t = tick_pool.acquire();
        if (t) tick_pool.release(t);
    }
    HFTTimer::cycle_t pool_end = HFTTimer::get_cycles();
    double pool_ns = HFTTimer::cycles_to_ns(pool_end - pool_start);
    std::cout << "Object pool acquire/release avg: " << (pool_ns / N) << " ns\n";

    // Checksum validation speed
    HFTTimer::cycle_t cksum_start = HFTTimer::get_cycles();
    uint16_t cksum = 0;
    for (size_t i = 0; i < N; ++i) {
        cksum ^= batch[i].checksum;
    }
    HFTTimer::cycle_t cksum_end = HFTTimer::get_cycles();
    double cksum_ns = HFTTimer::cycles_to_ns(cksum_end - cksum_start);
    std::cout << "Checksum validation avg: " << (cksum_ns / N) << " ns\n";

    // Message normalization overhead
    HFTTimer::cycle_t norm_start = HFTTimer::get_cycles();
    for (size_t i = 0; i < N; ++i) {
        auto t = MessageNormalizer::normalize_instrument_id(batch[i].instrument_id);
        (void)t;
    }
    HFTTimer::cycle_t norm_end = HFTTimer::get_cycles();
    double norm_ns = HFTTimer::cycles_to_ns(norm_end - norm_start);
    std::cout << "Normalization avg: " << (norm_ns / N) << " ns\n";

    // Throughput validation
    if (throughput > 1e6) {
        std::cout << "PASS: Throughput > 1M messages/sec\n";
    } else {
        std::cout << "FAIL: Throughput < 1M messages/sec\n";
    }
    return 0;
} 
