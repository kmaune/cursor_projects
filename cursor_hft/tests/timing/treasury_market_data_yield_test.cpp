#include <gtest/gtest.h>
#include "hft/market_data/treasury_instruments.hpp"

using namespace hft::market_data;

TEST(TreasuryMarketDataYield, Price32ndConversion) {
    double price = 99.515625; // 99-16.5
    auto p32 = Price32nd::from_decimal(price);
    EXPECT_NEAR(p32.to_decimal(), price, 1e-6);
    auto p32b = Price32nd::from_decimal(100.0);
    EXPECT_EQ(p32b.whole, 100);
    EXPECT_EQ(p32b.thirty_seconds, 0);
    EXPECT_EQ(p32b.half_32nds, 0);
}

TEST(TreasuryMarketDataYield, YieldCalculationRoundtrip) {
    TreasuryInstrument instr{TreasuryType::Note_5Y, 1825, 1'000'000};
    double price = 98.75;
    auto p32 = Price32nd::from_decimal(price);
    double yield = YieldCalculator::price_to_yield(instr, p32, 1825);
    auto p32b = YieldCalculator::yield_to_price(instr, yield, 1825);
    EXPECT_NEAR(p32b.to_decimal(), price, 0.01); // Acceptable for 4 decimals
}

TEST(TreasuryMarketDataYield, EdgeCases) {
    TreasuryInstrument instr{TreasuryType::Bill_3M, 90, 1'000'000};
    auto p32 = Price32nd::from_decimal(100.0);
    double yield = YieldCalculator::price_to_yield(instr, p32, 90);
    EXPECT_GE(yield, 0.0);
    auto p32b = YieldCalculator::yield_to_price(instr, yield, 90);
    EXPECT_NEAR(p32b.to_decimal(), 100.0, 0.01);
} 