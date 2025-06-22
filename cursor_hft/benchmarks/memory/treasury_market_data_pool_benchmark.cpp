#include <benchmark/benchmark.h>
#include <vector>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::market_data;

static void BM_TreasuryPool_TickAllocation(benchmark::State& state) {
    TreasuryTickPool tick_pool;
    constexpr size_t N = 4096;
    std::vector<TreasuryTick*> ticks(N);
    
    for (auto _ : state) {
        // Allocate
        for (size_t i = 0; i < N; ++i) {
            ticks[i] = tick_pool.acquire();
        }
        // Deallocate
        for (size_t i = 0; i < N; ++i) {
            if (ticks[i]) tick_pool.release(ticks[i]);
        }
    }
    
    state.SetItemsProcessed(N * state.iterations());
    state.SetLabel("Treasury tick pool allocation/deallocation");
}
BENCHMARK(BM_TreasuryPool_TickAllocation);

static void BM_TreasuryPool_TradeAllocation(benchmark::State& state) {
    TreasuryTradePool trade_pool;
    constexpr size_t N = 4096;
    std::vector<TreasuryTrade*> trades(N);
    
    for (auto _ : state) {
        // Allocate
        for (size_t i = 0; i < N; ++i) {
            trades[i] = trade_pool.acquire();
        }
        // Deallocate
        for (size_t i = 0; i < N; ++i) {
            if (trades[i]) trade_pool.release(trades[i]);
        }
    }
    
    state.SetItemsProcessed(N * state.iterations());
    state.SetLabel("Treasury trade pool allocation/deallocation");
}
BENCHMARK(BM_TreasuryPool_TradeAllocation);

static void BM_TreasuryPool_PriceLevelAllocation(benchmark::State& state) {
    // Simulate price level allocation performance
    constexpr size_t N = 4096;
    
    for (auto _ : state) {
        for (size_t i = 0; i < N; ++i) {
            auto price = Price32nd::from_decimal(100.0 + (i % 100) * 0.01);
            benchmark::DoNotOptimize(price);
        }
    }
    
    state.SetItemsProcessed(N * state.iterations());
    state.SetLabel("Price level allocation");
}
BENCHMARK(BM_TreasuryPool_PriceLevelAllocation);

static void BM_TreasuryPool_TickThroughput(benchmark::State& state) {
    TreasuryTickPool tick_pool;
    constexpr size_t ops_per_iteration = 1000;
    
    for (auto _ : state) {
        for (size_t i = 0; i < ops_per_iteration; ++i) {
            auto* tick = tick_pool.acquire();
            if (tick) {
                tick->instrument_type = TreasuryType::Note_2Y;
                tick->bid_price = Price32nd::from_decimal(99.5);
                tick->ask_price = Price32nd::from_decimal(99.515625);
                tick->bid_size = 10;
                tick->ask_size = 12;
                tick->bid_yield = 0.0250;
                tick->ask_yield = 0.0245;
                tick_pool.release(tick);
            }
        }
    }
    
    state.SetItemsProcessed(ops_per_iteration * state.iterations());
    state.SetLabel("Treasury tick throughput");
}
BENCHMARK(BM_TreasuryPool_TickThroughput);

BENCHMARK_MAIN(); 