#include <gtest/gtest.h>
#include "hft/market_data/treasury_instruments.hpp"

using namespace hft::market_data;

TEST(TreasuryMarketDataPool, TickPoolBasic) {
    TreasuryTickPool pool;
    std::vector<TreasuryTick*> ptrs;
    for (size_t i = 0; i < pool.capacity(); ++i) {
        auto* tick = pool.acquire();
        ASSERT_NE(tick, nullptr);
        ptrs.push_back(tick);
    }
    // Pool exhausted
    ASSERT_EQ(pool.acquire(), nullptr);
    // Release all
    for (auto* tick : ptrs) pool.release(tick);
    ASSERT_EQ(pool.size(), 0);
    // Re-acquire
    auto* tick2 = pool.acquire();
    ASSERT_NE(tick2, nullptr);
    pool.release(tick2);
}

TEST(TreasuryMarketDataPool, TradePoolBasic) {
    TreasuryTradePool pool;
    std::vector<TreasuryTrade*> ptrs;
    for (size_t i = 0; i < pool.capacity(); ++i) {
        auto* trade = pool.acquire();
        ASSERT_NE(trade, nullptr);
        ptrs.push_back(trade);
    }
    ASSERT_EQ(pool.acquire(), nullptr);
    for (auto* trade : ptrs) pool.release(trade);
    ASSERT_EQ(pool.size(), 0);
}

TEST(TreasuryMarketDataPool, Alignment) {
    TreasuryTickPool tick_pool;
    TreasuryTradePool trade_pool;
    auto* tick = tick_pool.acquire();
    auto* trade = trade_pool.acquire();
    ASSERT_EQ(reinterpret_cast<uintptr_t>(tick) % 64, 0u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(trade) % 64, 0u);
    tick_pool.release(tick);
    trade_pool.release(trade);
} 