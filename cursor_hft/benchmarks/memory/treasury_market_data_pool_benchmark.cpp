#include <iostream>
#include <vector>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::market_data;

int main() {
    TreasuryTickPool tick_pool;
    TreasuryTradePool trade_pool;
    constexpr size_t N = 4096;
    std::vector<TreasuryTick*> ticks(N);
    std::vector<TreasuryTrade*> trades(N);

    // Benchmark tick allocation
    auto start = hft::HFTTimer::get_timestamp_ns();
    for (size_t i = 0; i < N; ++i) ticks[i] = tick_pool.acquire();
    auto mid = hft::HFTTimer::get_timestamp_ns();
    for (size_t i = 0; i < N; ++i) tick_pool.release(ticks[i]);
    auto end = hft::HFTTimer::get_timestamp_ns();
    std::cout << "TreasuryTickPool allocation: " << (mid - start) << " ns for " << N << " objects\n";
    std::cout << "TreasuryTickPool deallocation: " << (end - mid) << " ns for " << N << " objects\n";

    // Benchmark trade allocation
    start = hft::HFTTimer::get_timestamp_ns();
    for (size_t i = 0; i < N; ++i) trades[i] = trade_pool.acquire();
    mid = hft::HFTTimer::get_timestamp_ns();
    for (size_t i = 0; i < N; ++i) trade_pool.release(trades[i]);
    end = hft::HFTTimer::get_timestamp_ns();
    std::cout << "TreasuryTradePool allocation: " << (mid - start) << " ns for " << N << " objects\n";
    std::cout << "TreasuryTradePool deallocation: " << (end - mid) << " ns for " << N << " objects\n";
    return 0;
} 