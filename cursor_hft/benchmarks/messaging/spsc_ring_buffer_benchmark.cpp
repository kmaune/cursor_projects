#include <benchmark/benchmark.h>
#include <thread>
#include <array>
#include <vector>
#include <hft/messaging/spsc_ring_buffer.hpp>
#include <hft/timing/hft_timer.hpp>

using namespace hft;

// Benchmark single push/pop operations
static void BM_SinglePushPop(benchmark::State& state) {
    SPSCRingBuffer<int, 1024> buffer;
    int value = 42;
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(buffer.try_push(value));
        benchmark::DoNotOptimize(buffer.try_pop(value));
    }
    
    state.SetItemsProcessed(state.iterations() * 2); // Count both push and pop
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(int));
}
BENCHMARK(BM_SinglePushPop)->UseRealTime();

// Benchmark batch operations
static void BM_BatchOperations(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    SPSCRingBuffer<int, 1024> buffer;
    std::vector<int> input(batch_size);
    std::vector<int> output(batch_size);
    
    // Fill input vector
    for (size_t i = 0; i < batch_size; ++i) {
        input[i] = static_cast<int>(i);
    }
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(buffer.try_push_batch(input.begin(), input.end()));
        benchmark::DoNotOptimize(buffer.try_pop_batch(output.begin(), output.end()));
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size * 2);
    state.SetBytesProcessed(state.iterations() * batch_size * 2 * sizeof(int));
}
BENCHMARK(BM_BatchOperations)
    ->RangeMultiplier(2)
    ->Range(1, 1024)
    ->UseRealTime();

// Benchmark producer/consumer latency (single round-trip)
static void BM_ProducerConsumerLatency(benchmark::State& state) {
    const size_t batch_size = state.range(0);
    SPSCRingBuffer<int, 1024> buffer;
    std::vector<int> input(batch_size);
    std::vector<int> output(batch_size);
    
    // Fill input vector
    for (size_t i = 0; i < batch_size; ++i) {
        input[i] = static_cast<int>(i);
    }
    
    for (auto _ : state) {
        // Measure push + pop latency
        auto start = HFTTimer::get_timestamp_ns();
        
        size_t pushed = buffer.try_push_batch(input.begin(), input.end());
        size_t popped = buffer.try_pop_batch(output.begin(), output.end());
        
        auto end = HFTTimer::get_timestamp_ns();
        
        benchmark::DoNotOptimize(pushed);
        benchmark::DoNotOptimize(popped);
        
        // Only count successful round-trips
        if (pushed == batch_size && popped == batch_size) {
            state.SetItemsProcessed(batch_size * 2); // push + pop
            state.counters["latency_ns"] = end - start;
        }
    }
    
    state.SetBytesProcessed(state.iterations() * batch_size * 2 * sizeof(int));
}
BENCHMARK(BM_ProducerConsumerLatency)
    ->RangeMultiplier(2)
    ->Range(1, 1024)
    ->UseRealTime();

// Benchmark latency distribution
static void BM_LatencyDistribution(benchmark::State& state) {
    SPSCRingBuffer<int, 1024> buffer;
    int value = 42;
    LatencyHistogram histogram;
    
    for (auto _ : state) {
        auto start = HFTTimer::get_timestamp_ns();
        benchmark::DoNotOptimize(buffer.try_push(value));
        benchmark::DoNotOptimize(buffer.try_pop(value));
        auto end = HFTTimer::get_timestamp_ns();
        
        histogram.record_latency(end - start);
    }
    
    // Get latency statistics
    auto stats = histogram.get_stats();
    state.counters["min_latency_ns"] = stats.min_latency;
    state.counters["max_latency_ns"] = stats.max_latency;
    state.counters["mean_latency_ns"] = stats.mean_latency;
    state.counters["p99_latency_ns"] = stats.percentiles[3];
    
    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * sizeof(int));
}
BENCHMARK(BM_LatencyDistribution)->UseRealTime();

// Benchmark with different message sizes
template<size_t MessageSize>
struct Message {
    char data[MessageSize];
};

template<size_t MessageSize>
static void BM_MessageSize(benchmark::State& state) {
    SPSCRingBuffer<Message<MessageSize>, 1024> buffer;
    Message<MessageSize> msg{};
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(buffer.try_push(msg));
        benchmark::DoNotOptimize(buffer.try_pop(msg));
    }
    
    state.SetItemsProcessed(state.iterations() * 2);
    state.SetBytesProcessed(state.iterations() * 2 * MessageSize);
}

BENCHMARK_TEMPLATE(BM_MessageSize, 8)->UseRealTime();
BENCHMARK_TEMPLATE(BM_MessageSize, 16)->UseRealTime();
BENCHMARK_TEMPLATE(BM_MessageSize, 32)->UseRealTime();
BENCHMARK_TEMPLATE(BM_MessageSize, 64)->UseRealTime();
BENCHMARK_TEMPLATE(BM_MessageSize, 128)->UseRealTime();

// Benchmark cache line effects
static void BM_CacheLineEffects(benchmark::State& state) {
    const size_t stride = state.range(0);
    SPSCRingBuffer<int, 1024> buffer;
    std::vector<int> data(1024);
    
    for (auto _ : state) {
        for (size_t i = 0; i < 1024; i += stride) {
            benchmark::DoNotOptimize(buffer.try_push(data[i]));
            benchmark::DoNotOptimize(buffer.try_pop(data[i]));
        }
    }
    
    state.SetItemsProcessed(state.iterations() * (1024 / stride) * 2);
    state.SetBytesProcessed(state.iterations() * (1024 / stride) * 2 * sizeof(int));
}
BENCHMARK(BM_CacheLineEffects)
    ->RangeMultiplier(2)
    ->Range(1, 64)
    ->UseRealTime();

BENCHMARK_MAIN();
