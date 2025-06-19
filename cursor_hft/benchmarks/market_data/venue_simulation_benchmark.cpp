#include <benchmark/benchmark.h>
#include "hft/market_data/venue_simulation.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/timing/hft_timer.hpp"
#include <memory>
#include <vector>

using namespace hft::market_data;

// Benchmark order submission latency
static void BM_OrderSubmission(benchmark::State& state) {
    PrimaryDealerVenue venue;
    TreasuryOrder order{};
    order.order_id = 1;
    order.instrument_type = TreasuryType::Note_10Y;
    order.order_type = OrderType::Limit;
    order.side = OrderSide::Buy;
    order.limit_price = Price32nd::from_decimal(99.0);
    order.quantity = 10;

    for (auto _ : state) {
        auto start = hft::HFTTimer::get_cycles();
        venue.submit_order(order);
        auto end = hft::HFTTimer::get_cycles();
        
        auto latency = hft::HFTTimer::cycles_to_ns(end - start);
        state.SetIterationTime(static_cast<double>(latency) / 1e9);
        order.order_id++;
    }
}
BENCHMARK(BM_OrderSubmission)->UseRealTime()->MinTime(5.0);

// Benchmark market data processing and fill generation
static void BM_MarketDataProcessing(benchmark::State& state) {
    PrimaryDealerVenue venue;
    
    // Setup some active orders
    std::vector<TreasuryOrder> orders;
    for (int i = 0; i < 100; ++i) {
        TreasuryOrder order{};
        order.order_id = i + 1;
        order.instrument_type = TreasuryType::Note_10Y;
        order.order_type = OrderType::Limit;
        order.side = (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell;
        order.limit_price = Price32nd::from_decimal(99.0 + (i % 2 == 0 ? -0.1 : 0.1));
        order.quantity = 10;
        venue.submit_order(order);
    }
    
    // Process responses to get to acknowledged state
    VenueResponse responses[256];
    while (venue.get_venue_responses(responses, 256) > 0) {}
    
    // Create market tick
    TreasuryTick tick{};
    tick.instrument_type = TreasuryType::Note_10Y;
    tick.timestamp_ns = hft::HFTTimer::get_timestamp_ns();
    tick.bid_price = Price32nd::from_decimal(98.9);
    tick.ask_price = Price32nd::from_decimal(99.1);
    tick.bid_size = 100;
    tick.ask_size = 100;
    
    for (auto _ : state) {
        auto start = hft::HFTTimer::get_cycles();
        venue.process_market_update(tick);
        auto end = hft::HFTTimer::get_cycles();
        
        auto latency = hft::HFTTimer::cycles_to_ns(end - start);
        state.SetIterationTime(static_cast<double>(latency) / 1e9);
        tick.timestamp_ns += 1'000'000; // 1ms between ticks
    }
}
BENCHMARK(BM_MarketDataProcessing)->UseRealTime()->MinTime(5.0);

// Benchmark order cancellation latency
static void BM_OrderCancellation(benchmark::State& state) {
    PrimaryDealerVenue venue;
    
    // Setup an active order
    TreasuryOrder order{};
    order.order_id = 1;
    order.instrument_type = TreasuryType::Note_10Y;
    order.order_type = OrderType::Limit;
    order.side = OrderSide::Buy;
    order.limit_price = Price32nd::from_decimal(99.0);
    order.quantity = 10;
    venue.submit_order(order);
    
    // Process acknowledgment
    VenueResponse responses[1];
    while (venue.get_venue_responses(responses, 1) > 0) {}
    
    for (auto _ : state) {
        auto start = hft::HFTTimer::get_cycles();
        venue.cancel_order(order.order_id);
        auto end = hft::HFTTimer::get_cycles();
        
        auto latency = hft::HFTTimer::cycles_to_ns(end - start);
        state.SetIterationTime(static_cast<double>(latency) / 1e9);
    }
}
BENCHMARK(BM_OrderCancellation)->UseRealTime()->MinTime(5.0);

// Benchmark order router throughput
static void BM_OrderRouterThroughput(benchmark::State& state) {
    TreasuryOrderRouter router;
    
    // Add multiple venues
    for (int i = 0; i < 4; ++i) {
        router.add_venue(std::make_unique<PrimaryDealerVenue>(
            "Venue" + std::to_string(i)));
    }
    
    // Create market tick
    TreasuryTick tick{};
    tick.instrument_type = TreasuryType::Note_10Y;
    tick.timestamp_ns = hft::HFTTimer::get_timestamp_ns();
    tick.bid_price = Price32nd::from_decimal(98.9);
    tick.ask_price = Price32nd::from_decimal(99.1);
    tick.bid_size = 100;
    tick.ask_size = 100;
    
    // Create test strategy
    TestMarketMakingStrategy strategy(router);
    
    for (auto _ : state) {
        auto start = hft::HFTTimer::get_cycles();
        
        // Process market data
        router.process_market_data(tick);
        strategy.process_market_tick(tick);
        
        // Process venue responses
        router.process_venue_responses();
        
        VenueResponse fills[256];
        size_t fill_count = router.get_fills(fills, 256);
        for (size_t i = 0; i < fill_count; ++i) {
            strategy.process_fill(fills[i]);
        }
        
        auto end = hft::HFTTimer::get_cycles();
        auto latency = hft::HFTTimer::cycles_to_ns(end - start);
        state.SetIterationTime(static_cast<double>(latency) / 1e9);
        
        tick.timestamp_ns += 1'000'000; // 1ms between ticks
    }
}
BENCHMARK(BM_OrderRouterThroughput)->UseRealTime()->MinTime(5.0);

BENCHMARK_MAIN(); 