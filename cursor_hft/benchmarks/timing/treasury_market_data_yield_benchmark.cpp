#include <benchmark/benchmark.h>
#include <random>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::market_data;

static void BM_YieldCalculation_PriceToYield(benchmark::State& state) {
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> price_dist(95.0, 105.0);
    
    TreasuryInstrument instrs[] = {
        TreasuryInstrument(TreasuryType::Bill_3M, 90, 1'000'000),
        TreasuryInstrument(TreasuryType::Bill_6M, 180, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_2Y, 730, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_5Y, 1825, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_10Y, 3650, 1'000'000),
        TreasuryInstrument(TreasuryType::Bond_30Y, 10950, 1'000'000)
    };
    
    size_t instr_index = state.range(0) % 6;
    auto& instr = instrs[instr_index];
    
    for (auto _ : state) {
        double price = price_dist(rng);
        auto p32 = Price32nd::from_decimal(price);
        double yield = YieldCalculator::price_to_yield(instr, p32, instr.maturity_days);
        benchmark::DoNotOptimize(yield);
    }
    
    state.SetLabel("Price to yield conversion");
}
BENCHMARK(BM_YieldCalculation_PriceToYield)->Arg(0)->Arg(1)->Arg(2)->Arg(3)->Arg(4)->Arg(5);

static void BM_YieldCalculation_YieldToPrice(benchmark::State& state) {
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> yield_dist(0.01, 0.05);
    
    TreasuryInstrument instrs[] = {
        TreasuryInstrument(TreasuryType::Bill_3M, 90, 1'000'000),
        TreasuryInstrument(TreasuryType::Bill_6M, 180, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_2Y, 730, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_5Y, 1825, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_10Y, 3650, 1'000'000),
        TreasuryInstrument(TreasuryType::Bond_30Y, 10950, 1'000'000)
    };
    
    size_t instr_index = state.range(0) % 6;
    auto& instr = instrs[instr_index];
    
    for (auto _ : state) {
        double yield = yield_dist(rng);
        auto p32 = YieldCalculator::yield_to_price(instr, yield, instr.maturity_days);
        benchmark::DoNotOptimize(p32);
    }
    
    state.SetLabel("Yield to price conversion");
}
BENCHMARK(BM_YieldCalculation_YieldToPrice)->Arg(0)->Arg(1)->Arg(2)->Arg(3)->Arg(4)->Arg(5);

static void BM_YieldCalculation_Throughput(benchmark::State& state) {
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> price_dist(95.0, 105.0);
    std::uniform_real_distribution<double> yield_dist(0.01, 0.05);
    
    TreasuryInstrument instr(TreasuryType::Note_10Y, 3650, 1'000'000);
    constexpr size_t ops_per_iteration = 1000;
    
    for (auto _ : state) {
        for (size_t i = 0; i < ops_per_iteration; ++i) {
            double price = price_dist(rng);
            auto p32 = Price32nd::from_decimal(price);
            double yield = YieldCalculator::price_to_yield(instr, p32, instr.maturity_days);
            auto p32_back = YieldCalculator::yield_to_price(instr, yield, instr.maturity_days);
            benchmark::DoNotOptimize(yield);
            benchmark::DoNotOptimize(p32_back);
        }
    }
    
    state.SetItemsProcessed(ops_per_iteration * state.iterations());
    state.SetLabel("Yield calculation throughput");
}
BENCHMARK(BM_YieldCalculation_Throughput);

BENCHMARK_MAIN(); 