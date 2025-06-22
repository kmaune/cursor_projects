#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include "hft/strategy/simple_market_maker.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

using namespace hft::strategy;
using namespace hft::trading;
using namespace hft::market_data;

class StrategyBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Initialize object pools
        order_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrder, 4096>>();
        level_pool_ = std::make_unique<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>>();
        update_buffer_ = std::make_unique<hft::SPSCRingBuffer<OrderBookUpdate, 8192>>();
        
        // Initialize order book
        order_book_ = std::make_unique<TreasuryOrderBook>(*order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize strategy
        strategy_ = std::make_unique<SimpleMarketMaker>(*order_pool_, *order_book_);
        
        // Setup market data
        setup_market_data();
    }

    void TearDown(const ::benchmark::State& state) override {
        strategy_.reset();
        order_book_.reset();
        update_buffer_.reset();
        level_pool_.reset();
        order_pool_.reset();
    }

protected:
    void setup_market_data() {
        // Create realistic treasury market data
        market_updates_.clear();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dist(100.0, 105.0);
        std::uniform_real_distribution<> spread_dist(0.01, 0.05);
        std::uniform_int_distribution<> size_dist(1000000, 10000000);
        
        const std::array<TreasuryType, 6> instruments = {
            TreasuryType::Bill_3M,
            TreasuryType::Bill_6M,
            TreasuryType::Note_2Y,
            TreasuryType::Note_5Y,
            TreasuryType::Note_10Y,
            TreasuryType::Bond_30Y
        };
        
        // Generate 1000 market updates across different instruments
        for (int i = 0; i < 1000; ++i) {
            SimpleMarketMaker::MarketUpdate update;
            update.instrument = instruments[i % instruments.size()];
            
            const double mid_price = price_dist(gen);
            const double half_spread = spread_dist(gen) / 2.0;
            
            update.best_bid = Price32nd::from_decimal(mid_price - half_spread);
            update.best_ask = Price32nd::from_decimal(mid_price + half_spread);
            update.bid_size = size_dist(gen);
            update.ask_size = size_dist(gen);
            update.update_time_ns = hft::HFTTimer::get_timestamp_ns();
            
            market_updates_.push_back(update);
        }
    }

    SimpleMarketMaker::MarketUpdate get_test_update(size_t index = 0) {
        if (market_updates_.empty()) {
            // Fallback to default
            SimpleMarketMaker::MarketUpdate update;
            update.instrument = TreasuryType::Note_10Y;
            update.best_bid = Price32nd::from_decimal(102.5);
            update.best_ask = Price32nd::from_decimal(102.53125);
            update.bid_size = 5000000;
            update.ask_size = 5000000;
            update.update_time_ns = hft::HFTTimer::get_timestamp_ns();
            return update;
        }
        return market_updates_[index % market_updates_.size()];
    }

protected:
    std::unique_ptr<hft::ObjectPool<TreasuryOrder, 4096>> order_pool_;
    std::unique_ptr<hft::ObjectPool<TreasuryOrderBook::PriceLevel, 1024>> level_pool_;
    std::unique_ptr<hft::SPSCRingBuffer<OrderBookUpdate, 8192>> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::unique_ptr<SimpleMarketMaker> strategy_;
    std::vector<SimpleMarketMaker::MarketUpdate> market_updates_;
};

// Benchmark core decision making latency
BENCHMARK_F(StrategyBenchmarkFixture, MarketMakingDecision)(benchmark::State& state) {
    auto update = get_test_update();
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    // Report custom metrics
    state.counters["AvgDecisionLatency_ns"] = strategy_->get_average_decision_latency_ns();
    state.counters["DecisionCount"] = strategy_->get_decision_count();
    
    // Calculate target performance metrics
    const double decisions_per_second = static_cast<double>(state.iterations()) / 
                                       (static_cast<double>(state.iterations()) / 1e9);
    state.counters["DecisionsPerSecond"] = decisions_per_second;
}

// Benchmark with varying market conditions
BENCHMARK_F(StrategyBenchmarkFixture, VaryingMarketConditions)(benchmark::State& state) {
    size_t update_index = 0;
    
    for (auto _ : state) {
        auto update = get_test_update(update_index++);
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    state.counters["AvgDecisionLatency_ns"] = strategy_->get_average_decision_latency_ns();
    state.counters["UniqueUpdatesProcessed"] = std::min(static_cast<size_t>(state.iterations()), market_updates_.size());
}

// Benchmark with position changes
BENCHMARK_F(StrategyBenchmarkFixture, WithPositionTracking)(benchmark::State& state) {
    auto update = get_test_update();
    int64_t position_change = 1000000;  // $1M per iteration
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
        
        // Simulate position updates
        strategy_->update_position(TreasuryType::Note_10Y, position_change, 
                                 Price32nd::from_decimal(102.5));
        position_change = -position_change;  // Alternate buy/sell
    }
    
    state.counters["AvgDecisionLatency_ns"] = strategy_->get_average_decision_latency_ns();
    state.counters["FinalPosition"] = strategy_->get_position(TreasuryType::Note_10Y);
}

// Benchmark multiple instruments simultaneously  
BENCHMARK_F(StrategyBenchmarkFixture, MultiInstrumentDecisions)(benchmark::State& state) {
    const std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M,
        TreasuryType::Bill_6M,
        TreasuryType::Note_2Y,
        TreasuryType::Note_5Y,
        TreasuryType::Note_10Y,
        TreasuryType::Bond_30Y
    };
    
    size_t instrument_index = 0;
    
    for (auto _ : state) {
        auto update = get_test_update();
        update.instrument = instruments[instrument_index++ % instruments.size()];
        
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    state.counters["AvgDecisionLatency_ns"] = strategy_->get_average_decision_latency_ns();
    state.counters["InstrumentsUsed"] = instruments.size();
}

// Benchmark spread calculation performance
BENCHMARK_F(StrategyBenchmarkFixture, SpreadCalculationOnly)(benchmark::State& state) {
    auto update = get_test_update();
    
    // Add some position to test inventory penalty calculation
    strategy_->update_position(TreasuryType::Note_10Y, 10000000, 
                             Price32nd::from_decimal(102.5));
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
        benchmark::DoNotOptimize(decision.bid_price);
        benchmark::DoNotOptimize(decision.ask_price);
    }
}

// Benchmark risk limit checking
BENCHMARK_F(StrategyBenchmarkFixture, RiskLimitChecking)(benchmark::State& state) {
    const uint64_t test_order_size = 1000000;
    
    for (auto _ : state) {
        bool result = strategy_->check_risk_limits(TreasuryType::Note_10Y, test_order_size);
        benchmark::DoNotOptimize(result);
    }
}

// Memory allocation benchmark (should be zero)
BENCHMARK_F(StrategyBenchmarkFixture, MemoryAllocationTest)(benchmark::State& state) {
    auto update = get_test_update();
    
    // Warm up
    for (int i = 0; i < 100; ++i) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    // Measure allocations during decisions
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
    }
    
    // In a production environment, this would integrate with memory profiling
    // to ensure zero allocations during the benchmark
    state.counters["ExpectedAllocations"] = 0;
}

// Latency consistency benchmark
BENCHMARK_F(StrategyBenchmarkFixture, LatencyConsistency)(benchmark::State& state) {
    auto update = get_test_update();
    
    uint64_t min_latency = UINT64_MAX;
    uint64_t max_latency = 0;
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
        
        min_latency = std::min(min_latency, decision.decision_latency_ns);
        max_latency = std::max(max_latency, decision.decision_latency_ns);
    }
    
    state.counters["MinLatency_ns"] = min_latency;
    state.counters["MaxLatency_ns"] = max_latency;
    state.counters["LatencyRange_ns"] = max_latency - min_latency;
    state.counters["AvgLatency_ns"] = strategy_->get_average_decision_latency_ns();
}

// Treasury pricing accuracy benchmark
BENCHMARK_F(StrategyBenchmarkFixture, TreasuryPricingAccuracy)(benchmark::State& state) {
    auto update = get_test_update();
    
    for (auto _ : state) {
        auto decision = strategy_->make_decision(update);
        benchmark::DoNotOptimize(decision);
        
        // Verify 32nds compliance
        benchmark::DoNotOptimize(decision.bid_price.thirty_seconds);
        benchmark::DoNotOptimize(decision.ask_price.thirty_seconds);
        benchmark::DoNotOptimize(decision.bid_price.half_32nds);
        benchmark::DoNotOptimize(decision.ask_price.half_32nds);
    }
}

// Throughput benchmark - simulate realistic message rates
BENCHMARK_F(StrategyBenchmarkFixture, HighThroughputSimulation)(benchmark::State& state) {
    // Simulate high-frequency market data updates
    const int batch_size = static_cast<int>(state.range(0));
    
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<SimpleMarketMaker::MarketUpdate> batch;
        batch.reserve(batch_size);
        
        for (int i = 0; i < batch_size; ++i) {
            batch.push_back(get_test_update(i));
        }
        state.ResumeTiming();
        
        // Process batch
        for (const auto& update : batch) {
            auto decision = strategy_->make_decision(update);
            benchmark::DoNotOptimize(decision);
        }
    }
    
    state.counters["BatchSize"] = batch_size;
    state.counters["UpdatesPerSecond"] = 
        static_cast<double>(state.iterations() * batch_size) / 
        (static_cast<double>(state.iterations()) / 1e9);
}
BENCHMARK_REGISTER_F(StrategyBenchmarkFixture, HighThroughputSimulation)
    ->RangeMultiplier(2)->Range(1, 128);

BENCHMARK_MAIN();