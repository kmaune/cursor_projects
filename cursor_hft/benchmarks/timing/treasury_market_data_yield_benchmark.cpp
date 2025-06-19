#include <iostream>
#include <random>
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::market_data;

int main() {
    constexpr size_t N = 1'000'000;
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> price_dist(95.0, 105.0);
    std::uniform_real_distribution<double> yield_dist(0.01, 0.05);
    TreasuryInstrument instrs[] = {
        TreasuryInstrument(TreasuryType::Bill_3M, 90, 1'000'000),
        TreasuryInstrument(TreasuryType::Bill_6M, 180, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_2Y, 730, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_5Y, 1825, 1'000'000),
        TreasuryInstrument(TreasuryType::Note_10Y, 3650, 1'000'000),
        TreasuryInstrument(TreasuryType::Bond_30Y, 10950, 1'000'000)
    };
    double min_ns = 1e9, max_ns = 0, sum_ns = 0;
    for (auto& instr : instrs) {
        for (size_t i = 0; i < N; ++i) {
            double price = price_dist(rng);
            auto p32 = Price32nd::from_decimal(price);
            auto start = hft::HFTTimer::get_cycles();
            double yield = YieldCalculator::price_to_yield(instr, p32, instr.maturity_days);
            auto end = hft::HFTTimer::get_cycles();
            double ns = hft::HFTTimer::cycles_to_ns(end - start);
            min_ns = std::min(min_ns, ns);
            max_ns = std::max(max_ns, ns);
            sum_ns += ns;
        }
        std::cout << "Instrument " << static_cast<int>(instr.type) << ": price_to_yield min/avg/max: "
                  << min_ns << "/" << (sum_ns/N) << "/" << max_ns << " ns\n";
        min_ns = 1e9; max_ns = 0; sum_ns = 0;
        for (size_t i = 0; i < N; ++i) {
            double yield = yield_dist(rng);
            auto start = hft::HFTTimer::get_cycles();
            auto p32 = YieldCalculator::yield_to_price(instr, yield, instr.maturity_days);
            auto end = hft::HFTTimer::get_cycles();
            double ns = hft::HFTTimer::cycles_to_ns(end - start);
            min_ns = std::min(min_ns, ns);
            max_ns = std::max(max_ns, ns);
            sum_ns += ns;
        }
        std::cout << "Instrument " << static_cast<int>(instr.type) << ": yield_to_price min/avg/max: "
                  << min_ns << "/" << (sum_ns/N) << "/" << max_ns << " ns\n";
        min_ns = 1e9; max_ns = 0; sum_ns = 0;
    }
    return 0;
} 