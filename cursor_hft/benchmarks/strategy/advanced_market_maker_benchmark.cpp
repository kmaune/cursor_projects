#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include "hft/strategy/advanced_market_maker.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::strategy;
using namespace hft::trading;
using namespace hft::market_data;

class AdvancedMarketMakerBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Initialize object pools
        order_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrder, 4096>>();
        level_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>>();
        update_buffer_ = std::make_unique<hft::SPSCRingBuffer<OrderBookUpdate, 8192>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize advanced market maker
        strategy_ = std::make_unique<AdvancedMarketMaker>(*order_pool_, *order_book_);
        
        // Initialize random number generator
        rng_.seed(42);
    }

    void TearDown(const ::benchmark::State& state) override {
        strategy_.reset();
        order_book_.reset();
        update_buffer_.reset();
        level_pool_.reset();
        order_pool_.reset();
    }

    AdvancedMarketMaker::MarketUpdate create_market_update(
        TreasuryType instrument = TreasuryType::Note_10Y,
        double base_price = 102.5,
        double volatility = 0.01
    ) {
        AdvancedMarketMaker::MarketUpdate update;
        
        // Add some price randomness for realistic testing
        std::normal_distribution<double> price_dist(0.0, volatility);
        const double price_offset = price_dist(rng_);
        const double bid_price = base_price + price_offset - 0.015625; // 1/64th spread
        const double ask_price = base_price + price_offset + 0.015625;
        
        update.instrument = instrument;
        update.best_bid = Price32nd::from_decimal(bid_price);
        update.best_ask = Price32nd::from_decimal(ask_price);
        update.bid_size = 5000000;
        update.ask_size = 5000000;
        update.update_time_ns = hft::HFTTimer::get_timestamp_ns();
        
        // Fill order book depth with realistic data
        for (size_t i = 0; i < AdvancedMarketMaker::MICROSTRUCTURE_LEVELS; ++i) {
            const double tick_increment = 0.015625; // 1/64th
            update.bid_levels[i] = Price32nd::from_decimal(bid_price - (i + 1) * tick_increment);
            update.ask_levels[i] = Price32nd::from_decimal(ask_price + (i + 1) * tick_increment);
            update.bid_sizes[i] = 5000000 / (i + 1);
            update.ask_sizes[i] = 5000000 / (i + 1);
        }
        
        return update;
    }

protected:
    std::unique_ptr<hft::ObjectPool<TreasuryOrder, 4096>> order_pool_;
    std::unique_ptr<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>> level_pool_;
    std::unique_ptr<hft::SPSCRingBuffer<OrderBookUpdate, 8192>> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<AdvancedMarketMaker> strategy_;
    std::mt19937 rng_;
};

// Benchmark decision making latency
BENCHMARK_F(AdvancedMarketMakerBenchmark, DecisionMakingLatency)(benchmark::State& state) {
    auto update = create_market_update();
    
    // Warm up
    for (int i = 0; i < 100; ++i) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark spread calculation performance
BENCHMARK_F(AdvancedMarketMakerBenchmark, SpreadCalculation)(benchmark::State& state) {
    auto update = create_market_update();
    strategy_->update_market_conditions(update);
    
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    for (auto _ : state) {
        auto spread = strategy_->calculate_optimal_spread(instrument);
        benchmark::DoNotOptimize(spread);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark position sizing calculation
BENCHMARK_F(AdvancedMarketMakerBenchmark, PositionSizing)(benchmark::State& state) {
    auto update = create_market_update();
    strategy_->update_market_conditions(update);
    
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    for (auto _ : state) {
        auto size = strategy_->calculate_position_size(instrument, 0.75);
        benchmark::DoNotOptimize(size);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark market condition updates
BENCHMARK_F(AdvancedMarketMakerBenchmark, MarketConditionUpdate)(benchmark::State& state) {
    auto update = create_market_update();
    
    for (auto _ : state) {
        strategy_->update_market_conditions(update);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark volatile market conditions
BENCHMARK_F(AdvancedMarketMakerBenchmark, VolatileMarketDecision)(benchmark::State& state) {
    // Create volatile market conditions
    for (int i = 0; i < 100; ++i) {
        auto volatile_update = create_market_update(TreasuryType::Note_10Y, 102.5, 0.1); // High volatility
        strategy_->update_market_conditions(volatile_update);
    }
    
    auto update = create_market_update(TreasuryType::Note_10Y, 102.5, 0.1);
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark multi-instrument coordination
BENCHMARK_F(AdvancedMarketMakerBenchmark, MultiInstrumentDecisions)(benchmark::State& state) {
    const std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M,
        TreasuryType::Bill_6M,
        TreasuryType::Note_2Y,
        TreasuryType::Note_5Y,
        TreasuryType::Note_10Y,
        TreasuryType::Bond_30Y
    };
    
    for (auto _ : state) {
        for (auto instrument : instruments) {
            auto update = create_market_update(instrument);
            auto decision = strategy_->make_decision(update);
            benchmark::DoNotOptimize(decision);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * instruments.size());
}

// Benchmark sustained decision making under load
BENCHMARK_F(AdvancedMarketMakerBenchmark, SustainedDecisionMaking)(benchmark::State& state) {
    // Simulate market updates with price movement
    std::vector<AdvancedMarketMaker::MarketUpdate> updates;
    updates.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        double price = 102.5 + 0.5 * std::sin(i * 0.01); // Sinusoidal price movement
        updates.push_back(create_market_update(TreasuryType::Note_10Y, price, 0.02));
    }
    
    size_t update_idx = 0;
    for (auto _ : state) {
        auto decision = strategy_->make_decision(updates[update_idx % updates.size()]);
        benchmark::DoNotOptimize(decision);
        ++update_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark inventory analysis
BENCHMARK_F(AdvancedMarketMakerBenchmark, InventoryAnalysis)(benchmark::State& state) {
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    for (auto _ : state) {
        strategy_->analyze_inventory(instrument);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark hedge ratio calculations
BENCHMARK_F(AdvancedMarketMakerBenchmark, HedgeRatioCalculation)(benchmark::State& state) {
    std::array<double, 6> yields;
    for (size_t i = 0; i < yields.size(); ++i) {
        yields[i] = 0.02 + 0.001 * i; // Realistic yield curve
    }
    
    for (auto _ : state) {
        strategy_->update_hedge_ratios(yields);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Benchmark comprehensive strategy workflow
BENCHMARK_F(AdvancedMarketMakerBenchmark, CompleteWorkflow)(benchmark::State& state) {
    auto update = create_market_update();
    const TreasuryType instrument = TreasuryType::Note_10Y;
    
    for (auto _ : state) {
        // Complete workflow: update conditions, make decision, analyze inventory
        strategy_->update_market_conditions(update);
        auto decision = strategy_->make_decision(update);
        strategy_->analyze_inventory(instrument);
        
        benchmark::DoNotOptimize(decision);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Performance targets validation
BENCHMARK_F(AdvancedMarketMakerBenchmark, PerformanceTargetValidation)(benchmark::State& state) {
    auto update = create_market_update();
    
    // Target: <500ns decision making
    const auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    const double avg_decision_time = static_cast<double>(total_duration.count()) / state.iterations();
    
    // Report average decision time
    state.counters["avg_decision_time_ns"] = avg_decision_time;
    state.SetItemsProcessed(state.iterations());
}

// Register benchmarks
BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, DecisionMakingLatency)
    ->Iterations(100000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, SpreadCalculation)
    ->Iterations(1000000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, PositionSizing)
    ->Iterations(1000000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, MarketConditionUpdate)
    ->Iterations(500000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, VolatileMarketDecision)
    ->Iterations(50000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, MultiInstrumentDecisions)
    ->Iterations(10000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, SustainedDecisionMaking)
    ->Iterations(50000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, InventoryAnalysis)
    ->Iterations(500000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, HedgeRatioCalculation)
    ->Iterations(100000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, CompleteWorkflow)
    ->Iterations(25000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_REGISTER_F(AdvancedMarketMakerBenchmark, PerformanceTargetValidation)
    ->Iterations(10000)
    ->ReportAggregatesOnly(true)
    ->DisplayAggregatesOnly(true);

BENCHMARK_MAIN();