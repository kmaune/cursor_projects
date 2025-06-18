#include "hft/timing/hft_timer.hpp"
#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include <random>

namespace hft {
namespace benchmark {

static void BM_GetCycles(::benchmark::State& state) {
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(HFTTimer::get_cycles());
    }
}
BENCHMARK(BM_GetCycles);

static void BM_GetTimestampNs(::benchmark::State& state) {
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(HFTTimer::get_timestamp_ns());
    }
}
BENCHMARK(BM_GetTimestampNs);

static void BM_CyclesToNs(::benchmark::State& state) {
    const auto cycles = HFTTimer::get_cycles();
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(HFTTimer::cycles_to_ns(cycles));
    }
}
BENCHMARK(BM_CyclesToNs);

static void BM_RecordLatency(::benchmark::State& state) {
    LatencyHistogram histogram;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<HFTTimer::ns_t> dist(100, 10000);
    
    for (auto _ : state) {
        histogram.record_latency(dist(gen));
    }
}
BENCHMARK(BM_RecordLatency);

static void BM_GetStats(::benchmark::State& state) {
    LatencyHistogram histogram;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<HFTTimer::ns_t> dist(100, 10000);
    
    // Pre-populate with some data
    for (int i = 0; i < 1000000; ++i) {
        histogram.record_latency(dist(gen));
    }
    
    for (auto _ : state) {
        ::benchmark::DoNotOptimize(histogram.get_stats());
    }
}
BENCHMARK(BM_GetStats);

static void BM_ScopedTimer(::benchmark::State& state) {
    LatencyHistogram histogram;
    for (auto _ : state) {
        HFTTimer::ScopedTimer timer(histogram);
        ::benchmark::DoNotOptimize(timer);
    }
}
BENCHMARK(BM_ScopedTimer);

static void BM_ConcurrentRecording(::benchmark::State& state) {
    LatencyHistogram histogram;
    constexpr size_t NUM_THREADS = 4;
    std::vector<std::thread> threads;
    
    for (auto _ : state) {
        threads.clear();
        for (size_t i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back([&histogram]() {
                for (int j = 0; j < 1000; ++j) {
                    const auto start = HFTTimer::get_cycles();
                    const auto end = HFTTimer::get_cycles();
                    histogram.record_latency(HFTTimer::cycles_to_ns(end - start));
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
}
BENCHMARK(BM_ConcurrentRecording);

static void BM_HistogramReset(::benchmark::State& state) {
    LatencyHistogram histogram;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<HFTTimer::ns_t> dist(100, 10000);
    
    // Pre-populate with some data
    for (int i = 0; i < 1000000; ++i) {
        histogram.record_latency(dist(gen));
    }
    
    for (auto _ : state) {
        histogram.reset();
    }
}
BENCHMARK(BM_HistogramReset);

} // namespace benchmark
} // namespace hft

BENCHMARK_MAIN(); 