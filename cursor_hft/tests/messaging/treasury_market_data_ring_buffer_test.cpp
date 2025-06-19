#include <gtest/gtest.h>
#include "hft/market_data/treasury_instruments.hpp"

using namespace hft::market_data;

TEST(TreasuryMarketDataRingBuffer, TickBufferBasic) {
    TreasuryTickBuffer buffer;
    TreasuryTick tick{};
    tick.instrument_type = TreasuryType::Note_2Y;
    tick.bid_price = Price32nd::from_decimal(99.5);
    tick.ask_price = Price32nd::from_decimal(99.515625);
    tick.bid_size = 10;
    tick.ask_size = 12;
    tick.bid_yield = 0.0250;
    tick.ask_yield = 0.0245;
    size_t count = 0;
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        ASSERT_TRUE(buffer.try_push(tick));
        ++count;
    }
    ASSERT_FALSE(buffer.try_push(tick)); // Buffer full
    TreasuryTick out;
    for (size_t i = 0; i < count; ++i) {
        ASSERT_TRUE(buffer.try_pop(out));
        ASSERT_EQ(out.bid_price.whole, 99);
    }
    ASSERT_FALSE(buffer.try_pop(out)); // Buffer empty
}

TEST(TreasuryMarketDataRingBuffer, TradeBufferBasic) {
    TreasuryTradeBuffer buffer;
    TreasuryTrade trade{};
    trade.instrument_type = TreasuryType::Bond_30Y;
    trade.trade_price = Price32nd::from_decimal(101.0);
    trade.trade_size = 5;
    trade.trade_yield = 0.0310;
    std::strncpy(trade.trade_id, "TST123", 16);
    size_t count = 0;
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        ASSERT_TRUE(buffer.try_push(trade));
        ++count;
    }
    ASSERT_FALSE(buffer.try_push(trade));
    TreasuryTrade out;
    for (size_t i = 0; i < count; ++i) {
        ASSERT_TRUE(buffer.try_pop(out));
        ASSERT_EQ(out.trade_price.whole, 101);
    }
    ASSERT_FALSE(buffer.try_pop(out));
} 