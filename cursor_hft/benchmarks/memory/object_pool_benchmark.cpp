// ObjectPool Benchmark for HFT
// Benchmarks allocation/deallocation latency, batch performance, memory usage, and object size impact
// HFT targets: <10ns non-timed, <25ns timed allocation
//
// Copyright (c) 2024

#include <benchmark/benchmark.h>
#include <hft/memory/object_pool.hpp>
#include <hft/timing/hft_timer.hpp>
#include <cstdint>
#include <vector>
#include <random>
#include <cstring>

using namespace hft;

struct alignas(64) SmallObject {
    uint64_t data[1];
};

struct alignas(64) LargeObject {
    uint64_t data[8];
};

constexpr size_t POOL_SIZE = 4096;

// Non-timed pool
static void BM_ObjectPool_AcquireRelease_Small_NonTimed(benchmark::State& state) {
    ObjectPool<SmallObject, POOL_SIZE, false> pool;
    for (auto _ : state) {
        SmallObject* obj = pool.acquire();
        benchmark::DoNotOptimize(obj);
        pool.release(obj);
    }
    state.counters["ns_per_op"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_ObjectPool_AcquireRelease_Small_NonTimed)->MinTime(0.1);

// Timed pool
static void BM_ObjectPool_AcquireRelease_Small_Timed(benchmark::State& state) {
    ObjectPool<SmallObject, POOL_SIZE, true> pool;
    for (auto _ : state) {
        SmallObject* obj = pool.acquire();
        benchmark::DoNotOptimize(obj);
        pool.release(obj);
    }
    state.counters["ns_per_op"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_ObjectPool_AcquireRelease_Small_Timed)->MinTime(0.1);

// Large object, non-timed
static void BM_ObjectPool_AcquireRelease_Large_NonTimed(benchmark::State& state) {
    ObjectPool<LargeObject, POOL_SIZE, false> pool;
    for (auto _ : state) {
        LargeObject* obj = pool.acquire();
        benchmark::DoNotOptimize(obj);
        pool.release(obj);
    }
    state.counters["ns_per_op"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_ObjectPool_AcquireRelease_Large_NonTimed)->MinTime(0.1);

// Large object, timed
static void BM_ObjectPool_AcquireRelease_Large_Timed(benchmark::State& state) {
    ObjectPool<LargeObject, POOL_SIZE, true> pool;
    for (auto _ : state) {
        LargeObject* obj = pool.acquire();
        benchmark::DoNotOptimize(obj);
        pool.release(obj);
    }
    state.counters["ns_per_op"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_ObjectPool_AcquireRelease_Large_Timed)->MinTime(0.1);

// Batch allocation/deallocation
static void BM_ObjectPool_Batch_Small(benchmark::State& state) {
    ObjectPool<SmallObject, POOL_SIZE, false> pool;
    std::vector<SmallObject*> ptrs(POOL_SIZE);
    for (auto _ : state) {
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            ptrs[i] = pool.acquire();
        }
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            pool.release(ptrs[i]);
        }
    }
}
BENCHMARK(BM_ObjectPool_Batch_Small)->MinTime(0.1);

// Memory usage validation
static void BM_ObjectPool_ValidateMemory(benchmark::State& state) {
    ObjectPool<SmallObject, POOL_SIZE, false> pool;
    for (auto _ : state) {
        benchmark::DoNotOptimize(pool.validate_memory());
    }
}
BENCHMARK(BM_ObjectPool_ValidateMemory)->MinTime(0.1);

BENCHMARK_MAIN(); 