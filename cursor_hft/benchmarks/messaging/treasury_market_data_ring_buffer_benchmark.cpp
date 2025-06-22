#include <benchmark/benchmark.h>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::market_data;

static void BM_TreasuryRingBuffer_TickPushPop(benchmark::State& state) {
    TreasuryTickBuffer tick_buffer;
    constexpr size_t N = 8192;
    TreasuryTick tick{};
    tick.instrument_type = TreasuryType::Note_2Y;
    tick.bid_price = Price32nd::from_decimal(99.5);
    tick.ask_price = Price32nd::from_decimal(99.515625);
    tick.bid_size = 10;
    tick.ask_size = 12;
    tick.bid_yield = 0.0250;
    tick.ask_yield = 0.0245;
    
    for (auto _ : state) {
        // Push
        for (size_t i = 0; i < N; ++i) {
            tick_buffer.try_push(tick);
        }
        // Pop
        TreasuryTick out;
        for (size_t i = 0; i < N; ++i) {
            tick_buffer.try_pop(out);
        }
    }
    
    state.SetItemsProcessed(N * state.iterations());
    state.SetLabel("Treasury tick buffer push/pop");
}
BENCHMARK(BM_TreasuryRingBuffer_TickPushPop);

static void BM_TreasuryRingBuffer_TradePushPop(benchmark::State& state) {
    TreasuryTradeBuffer trade_buffer;
    constexpr size_t N = 8192;
    TreasuryTrade trade{};
    trade.instrument_type = TreasuryType::Bond_30Y;
    trade.trade_price = Price32nd::from_decimal(101.0);
    trade.trade_size = 5;
    trade.trade_yield = 0.0310;
    std::strncpy(trade.trade_id, "TST123", 16);
    
    for (auto _ : state) {
        // Push
        for (size_t i = 0; i < N; ++i) {
            trade_buffer.try_push(trade);
        }
        // Pop
        TreasuryTrade out;
        for (size_t i = 0; i < N; ++i) {
            trade_buffer.try_pop(out);
        }
    }
    
    state.SetItemsProcessed(N * state.iterations());
    state.SetLabel("Treasury trade buffer push/pop");
}
BENCHMARK(BM_TreasuryRingBuffer_TradePushPop);

static void BM_TreasuryRingBuffer_MarketDataMessage(benchmark::State& state) {
    TreasuryTickBuffer tick_buffer;
    constexpr size_t N = 1000;
    TreasuryTick tick{};
    tick.instrument_type = TreasuryType::Note_10Y;
    tick.bid_price = Price32nd::from_decimal(100.0);
    tick.ask_price = Price32nd::from_decimal(100.03125);
    tick.bid_size = 100;
    tick.ask_size = 100;
    tick.bid_yield = 0.0300;
    tick.ask_yield = 0.0295;
    
    for (auto _ : state) {
        for (size_t i = 0; i < N; ++i) {
            tick_buffer.try_push(tick);
            TreasuryTick out;
            tick_buffer.try_pop(out);
            benchmark::DoNotOptimize(out);
        }
    }
    
    state.SetItemsProcessed(N * state.iterations());
    state.SetLabel("Market data message processing");
}
BENCHMARK(BM_TreasuryRingBuffer_MarketDataMessage);

static void BM_TreasuryRingBuffer_TickThroughput(benchmark::State& state) {
    TreasuryTickBuffer tick_buffer;
    constexpr size_t N = 10000;
    TreasuryTick tick{};
    tick.instrument_type = TreasuryType::Bill_3M;
    tick.bid_price = Price32nd::from_decimal(99.875);
    tick.ask_price = Price32nd::from_decimal(99.90625);
    tick.bid_size = 50;
    tick.ask_size = 50;
    tick.bid_yield = 0.0150;
    tick.ask_yield = 0.0145;
    
    for (auto _ : state) {
        for (size_t i = 0; i < N; ++i) {
            tick_buffer.try_push(tick);
        }
    }
    
    state.SetItemsProcessed(N * state.iterations());
    state.SetLabel("Treasury tick throughput");
}
BENCHMARK(BM_TreasuryRingBuffer_TickThroughput);

BENCHMARK_MAIN(); 