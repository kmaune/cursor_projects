#include <iostream>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::market_data;

int main() {
    TreasuryTickBuffer tick_buffer;
    TreasuryTradeBuffer trade_buffer;
    constexpr size_t N = 8192;
    TreasuryTick tick{};
    tick.instrument_type = TreasuryType::Note_2Y;
    tick.bid_price = Price32nd::from_decimal(99.5);
    tick.ask_price = Price32nd::from_decimal(99.515625);
    tick.bid_size = 10;
    tick.ask_size = 12;
    tick.bid_yield = 0.0250;
    tick.ask_yield = 0.0245;
    TreasuryTrade trade{};
    trade.instrument_type = TreasuryType::Bond_30Y;
    trade.trade_price = Price32nd::from_decimal(101.0);
    trade.trade_size = 5;
    trade.trade_yield = 0.0310;
    std::strncpy(trade.trade_id, "TST123", 16);

    // Benchmark tick buffer push
    auto start = hft::HFTTimer::get_timestamp_ns();
    for (size_t i = 0; i < N; ++i) tick_buffer.try_push(tick);
    auto mid = hft::HFTTimer::get_timestamp_ns();
    TreasuryTick out;
    for (size_t i = 0; i < N; ++i) tick_buffer.try_pop(out);
    auto end = hft::HFTTimer::get_timestamp_ns();
    std::cout << "TreasuryTickBuffer push: " << (mid - start) << " ns for " << N << " ops\n";
    std::cout << "TreasuryTickBuffer pop: " << (end - mid) << " ns for " << N << " ops\n";

    // Benchmark trade buffer push
    start = hft::HFTTimer::get_timestamp_ns();
    for (size_t i = 0; i < N; ++i) trade_buffer.try_push(trade);
    mid = hft::HFTTimer::get_timestamp_ns();
    TreasuryTrade out2;
    for (size_t i = 0; i < N; ++i) trade_buffer.try_pop(out2);
    end = hft::HFTTimer::get_timestamp_ns();
    std::cout << "TreasuryTradeBuffer push: " << (mid - start) << " ns for " << N << " ops\n";
    std::cout << "TreasuryTradeBuffer pop: " << (end - mid) << " ns for " << N << " ops\n";
    return 0;
} 