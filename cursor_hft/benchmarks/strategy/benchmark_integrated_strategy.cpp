#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <memory>

#include "hft/strategy/integrated_market_making_strategy.hpp"
#include "hft/strategy/treasury_quote_manager.hpp"
#include "hft/strategy/inventory_manager.hpp"
#include "hft/strategy/risk_integration.hpp"
#include "hft/strategy/market_data_processor.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::strategy;
using namespace hft::market_data;
using namespace hft::trading;

// Global test infrastructure
static std::unique_ptr<ObjectPool<TreasuryOrder, 10000>> g_order_pool;
static std::unique_ptr<ObjectPool<MarketUpdate, 10000>> g_update_pool;
static std::unique_ptr<TreasuryOrderBook> g_order_book;
static std::unique_ptr<IntegratedMarketMakingStrategy> g_strategy;
static std::unique_ptr<TreasuryQuoteManager> g_quote_manager;
static std::unique_ptr<InventoryManager> g_inventory_manager;
static std::unique_ptr<RiskIntegrationFramework> g_risk_framework;
static std::unique_ptr<MarketDataProcessor> g_market_data_processor;

// Random number generation for realistic market data
static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_real_distribution<double> price_dist(100.0, 105.0);
static std::uniform_int_distribution<uint64_t> size_dist(500000, 5000000);
static std::uniform_int_distribution<int> instrument_dist(0, 5);

// Helper functions
MarketUpdate generate_random_market_update() {
    auto instrument = static_cast<TreasuryType>(instrument_dist(gen));
    double bid_price = price_dist(gen);
    double ask_price = bid_price + 0.03125 + (price_dist(gen) - 102.5) * 0.01; // Small spread with random component
    
    return MarketUpdate(
        instrument,
        Price32nd::from_decimal(bid_price),
        Price32nd::from_decimal(ask_price),
        size_dist(gen),
        size_dist(gen)
    );
}

QuoteUpdateRequest generate_random_quote_request() {
    auto instrument = static_cast<TreasuryType>(instrument_dist(gen));
    double bid_price = std::floor(price_dist(gen) * 32.0) / 32.0; // Align to 32nds
    double ask_price = bid_price + 0.03125; // 1/32nd spread
    
    return QuoteUpdateRequest(
        instrument,
        Price32nd::from_decimal(bid_price),
        Price32nd::from_decimal(ask_price),
        size_dist(gen),
        size_dist(gen)
    );
}

// Setup/teardown for benchmarks
static void SetupBenchmarks() {
    g_order_pool = std::make_unique<ObjectPool<TreasuryOrder, 10000>>();
    g_update_pool = std::make_unique<ObjectPool<MarketUpdate, 10000>>();
    g_order_book = std::make_unique<TreasuryOrderBook>(*g_order_pool);
    
    g_strategy = std::make_unique<IntegratedMarketMakingStrategy>(
        *g_order_pool, *g_update_pool, *g_order_book);
    g_quote_manager = std::make_unique<TreasuryQuoteManager>(*g_order_pool, *g_order_book);
    g_inventory_manager = std::make_unique<InventoryManager>();
    g_risk_framework = std::make_unique<RiskIntegrationFramework>();
    g_market_data_processor = std::make_unique<MarketDataProcessor>();
}

static void TeardownBenchmarks() {
    g_market_data_processor.reset();
    g_risk_framework.reset();
    g_inventory_manager.reset();
    g_quote_manager.reset();
    g_strategy.reset();
    g_order_book.reset();
    g_update_pool.reset();
    g_order_pool.reset();
}

// ===== INTEGRATED STRATEGY BENCHMARKS =====

static void BM_IntegratedStrategy_DecisionMaking(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<MarketUpdate> test_updates;
    test_updates.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        test_updates.push_back(generate_random_market_update());
    }
    
    size_t update_idx = 0;
    
    for (auto _ : state) {
        auto decision = g_strategy->make_integrated_decision(test_updates[update_idx % test_updates.size()]);
        benchmark::DoNotOptimize(decision);
        ++update_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_IntegratedStrategy_DecisionMaking)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(100000)
    ->ReportAggregatesOnly(true);

static void BM_IntegratedStrategy_PositionUpdate(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<std::tuple<TreasuryType, int64_t, Price32nd>> position_updates;
    position_updates.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        position_updates.emplace_back(
            static_cast<TreasuryType>(instrument_dist(gen)),
            (size_dist(gen) / 1000) * (gen() % 2 == 0 ? 1 : -1), // Random +/- position
            Price32nd::from_decimal(price_dist(gen))
        );
    }
    
    size_t update_idx = 0;
    
    for (auto _ : state) {
        auto& [instrument, quantity, price] = position_updates[update_idx % position_updates.size()];
        g_strategy->update_position(instrument, quantity, price);
        ++update_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_IntegratedStrategy_PositionUpdate)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(50000);

// ===== TREASURY QUOTE MANAGER BENCHMARKS =====

static void BM_QuoteManager_Validation(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<QuoteUpdateRequest> test_requests;
    test_requests.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        test_requests.push_back(generate_random_quote_request());
    }
    
    size_t request_idx = 0;
    
    for (auto _ : state) {
        auto result = g_quote_manager->validate_quote(test_requests[request_idx % test_requests.size()]);
        benchmark::DoNotOptimize(result);
        ++request_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_QuoteManager_Validation)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(100000);

static void BM_QuoteManager_ProcessUpdate(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<QuoteUpdateRequest> test_requests;
    test_requests.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        test_requests.push_back(generate_random_quote_request());
    }
    
    size_t request_idx = 0;
    
    for (auto _ : state) {
        state.PauseTiming();
        auto request = test_requests[request_idx % test_requests.size()];
        request.request_time_ns = hft::HFTTimer::get_timestamp_ns();
        ++request_idx;
        state.ResumeTiming();
        
        auto result = g_quote_manager->process_quote_update(request);
        benchmark::DoNotOptimize(result);
        
        state.PauseTiming();
        // Clean up for next iteration
        g_quote_manager->cancel_quotes(request.instrument);
        state.ResumeTiming();
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_QuoteManager_ProcessUpdate)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(10000);

// ===== INVENTORY MANAGER BENCHMARKS =====

static void BM_InventoryManager_PositionUpdate(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<TradeExecution> test_executions;
    test_executions.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        test_executions.emplace_back(
            static_cast<TreasuryType>(instrument_dist(gen)),
            (size_dist(gen) / 1000) * (gen() % 2 == 0 ? 1 : -1),
            Price32nd::from_decimal(price_dist(gen))
        );
    }
    
    size_t execution_idx = 0;
    
    for (auto _ : state) {
        auto result = g_inventory_manager->update_position(test_executions[execution_idx % test_executions.size()]);
        benchmark::DoNotOptimize(result);
        ++execution_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_InventoryManager_PositionUpdate)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(50000);

static void BM_InventoryManager_InventoryAdjustment(benchmark::State& state) {
    SetupBenchmarks();
    
    for (auto _ : state) {
        auto instrument = static_cast<TreasuryType>(instrument_dist(gen));
        auto adjustment = g_inventory_manager->calculate_inventory_adjustment(instrument);
        benchmark::DoNotOptimize(adjustment);
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_InventoryManager_InventoryAdjustment)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(100000);

// ===== RISK INTEGRATION BENCHMARKS =====

static void BM_RiskFramework_Layer1Check(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<RiskCheckRequest> test_requests;
    test_requests.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        test_requests.emplace_back(
            static_cast<TreasuryType>(instrument_dist(gen)),
            (size_dist(gen) / 1000) * (gen() % 2 == 0 ? 1 : -1),
            Price32nd::from_decimal(price_dist(gen))
        );
    }
    
    size_t request_idx = 0;
    
    for (auto _ : state) {
        auto result = g_risk_framework->perform_layer1_check(test_requests[request_idx % test_requests.size()]);
        benchmark::DoNotOptimize(result);
        ++request_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_RiskFramework_Layer1Check)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(100000);

static void BM_RiskFramework_Layer2Check(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<RiskCheckRequest> test_requests;
    test_requests.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        test_requests.emplace_back(
            static_cast<TreasuryType>(instrument_dist(gen)),
            (size_dist(gen) / 1000) * (gen() % 2 == 0 ? 1 : -1),
            Price32nd::from_decimal(price_dist(gen))
        );
    }
    
    size_t request_idx = 0;
    
    for (auto _ : state) {
        auto result = g_risk_framework->perform_layer2_check(test_requests[request_idx % test_requests.size()]);
        benchmark::DoNotOptimize(result);
        ++request_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_RiskFramework_Layer2Check)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(50000);

static void BM_RiskFramework_ComprehensiveCheck(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<RiskCheckRequest> test_requests;
    test_requests.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        RiskCheckRequest request(
            static_cast<TreasuryType>(instrument_dist(gen)),
            (size_dist(gen) / 1000) * (gen() % 2 == 0 ? 1 : -1),
            Price32nd::from_decimal(price_dist(gen)),
            true // Enable enhanced checks
        );
        test_requests.push_back(request);
    }
    
    size_t request_idx = 0;
    
    for (auto _ : state) {
        auto result = g_risk_framework->perform_comprehensive_check(test_requests[request_idx % test_requests.size()]);
        benchmark::DoNotOptimize(result);
        ++request_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_RiskFramework_ComprehensiveCheck)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(25000);

// ===== MARKET DATA PROCESSOR BENCHMARKS =====

static void BM_MarketDataProcessor_Level1Update(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<Level1Snapshot> test_snapshots;
    test_snapshots.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        Level1Snapshot snapshot;
        snapshot.instrument = static_cast<TreasuryType>(instrument_dist(gen));
        snapshot.best_bid = Price32nd::from_decimal(price_dist(gen));
        snapshot.best_ask = Price32nd::from_decimal(snapshot.best_bid.to_decimal() + 0.03125);
        snapshot.bid_size = size_dist(gen);
        snapshot.ask_size = size_dist(gen);
        snapshot.snapshot_time = hft::HFTTimer::get_timestamp_ns();
        test_snapshots.push_back(snapshot);
    }
    
    size_t snapshot_idx = 0;
    
    for (auto _ : state) {
        auto& snapshot = test_snapshots[snapshot_idx % test_snapshots.size()];
        snapshot.snapshot_time = hft::HFTTimer::get_timestamp_ns(); // Update timestamp
        auto result = g_market_data_processor->update_level1_data(snapshot);
        benchmark::DoNotOptimize(result);
        ++snapshot_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_MarketDataProcessor_Level1Update)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(100000);

static void BM_MarketDataProcessor_SignalAnalysis(benchmark::State& state) {
    SetupBenchmarks();
    
    // Pre-populate with some market data
    for (int i = 0; i < 100; ++i) {
        Level1Snapshot snapshot;
        snapshot.instrument = static_cast<TreasuryType>(i % 6);
        snapshot.best_bid = Price32nd::from_decimal(price_dist(gen));
        snapshot.best_ask = Price32nd::from_decimal(snapshot.best_bid.to_decimal() + 0.03125);
        snapshot.bid_size = size_dist(gen);
        snapshot.ask_size = size_dist(gen);
        snapshot.snapshot_time = hft::HFTTimer::get_timestamp_ns();
        g_market_data_processor->update_level1_data(snapshot);
    }
    
    for (auto _ : state) {
        auto instrument = static_cast<TreasuryType>(instrument_dist(gen));
        auto analysis = g_market_data_processor->analyze_market_signals(instrument);
        benchmark::DoNotOptimize(analysis);
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_MarketDataProcessor_SignalAnalysis)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(50000);

// ===== INTEGRATED WORKFLOW BENCHMARKS =====

static void BM_EndToEnd_TickToTrade_Simulation(benchmark::State& state) {
    SetupBenchmarks();
    
    std::vector<MarketUpdate> test_updates;
    test_updates.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        test_updates.push_back(generate_random_market_update());
    }
    
    size_t update_idx = 0;
    
    for (auto _ : state) {
        // Simulate full tick-to-trade workflow
        auto& market_update = test_updates[update_idx % test_updates.size()];
        
        // 1. Market data processing
        Level1Snapshot snapshot;
        snapshot.instrument = market_update.instrument;
        snapshot.best_bid = market_update.best_bid;
        snapshot.best_ask = market_update.best_ask;
        snapshot.bid_size = market_update.bid_size;
        snapshot.ask_size = market_update.ask_size;
        snapshot.snapshot_time = hft::HFTTimer::get_timestamp_ns();
        
        auto md_result = g_market_data_processor->update_level1_data(snapshot);
        benchmark::DoNotOptimize(md_result);
        
        // 2. Strategy decision
        auto decision = g_strategy->make_integrated_decision(market_update);
        benchmark::DoNotOptimize(decision);
        
        // 3. Risk check (if updating quotes)
        if (decision.action == TradingDecision::Action::UPDATE_QUOTES) {
            RiskCheckRequest risk_request(
                decision.instrument,
                static_cast<int64_t>(decision.bid_size),
                decision.bid_price,
                true
            );
            auto risk_result = g_risk_framework->perform_comprehensive_check(risk_request);
            benchmark::DoNotOptimize(risk_result);
            
            // 4. Quote management (if risk approved)
            if (risk_result.result == RiskCheckResult::APPROVED) {
                QuoteUpdateRequest quote_request(
                    decision.instrument,
                    decision.bid_price,
                    decision.ask_price,
                    decision.bid_size,
                    decision.ask_size
                );
                auto quote_result = g_quote_manager->process_quote_update(quote_request);
                benchmark::DoNotOptimize(quote_result);
            }
        }
        
        ++update_idx;
    }
    
    state.SetItemsProcessed(state.iterations());
    
    TeardownBenchmarks();
}
BENCHMARK(BM_EndToEnd_TickToTrade_Simulation)
    ->Unit(benchmark::kNanosecond)
    ->Iterations(10000)
    ->ReportAggregatesOnly(true);

// Performance targets as custom counters
static void BM_PerformanceTargets_Validation(benchmark::State& state) {
    SetupBenchmarks();
    
    constexpr int num_samples = 10000;
    std::vector<uint64_t> decision_latencies;
    std::vector<uint64_t> quote_latencies;
    std::vector<uint64_t> risk_latencies;
    
    decision_latencies.reserve(num_samples);
    quote_latencies.reserve(num_samples);
    risk_latencies.reserve(num_samples);
    
    for (int i = 0; i < num_samples && state.KeepRunning(); ++i) {
        // Strategy decision latency
        auto market_update = generate_random_market_update();
        auto start = hft::HFTTimer::get_timestamp_ns();
        auto decision = g_strategy->make_integrated_decision(market_update);
        auto end = hft::HFTTimer::get_timestamp_ns();
        decision_latencies.push_back(end - start);
        
        // Quote processing latency
        auto quote_request = generate_random_quote_request();
        quote_request.request_time_ns = hft::HFTTimer::get_timestamp_ns();
        start = hft::HFTTimer::get_timestamp_ns();
        auto quote_result = g_quote_manager->process_quote_update(quote_request);
        end = hft::HFTTimer::get_timestamp_ns();
        quote_latencies.push_back(end - start);
        
        // Risk check latency
        RiskCheckRequest risk_request(
            static_cast<TreasuryType>(instrument_dist(gen)),
            size_dist(gen) / 1000,
            Price32nd::from_decimal(price_dist(gen)),
            true
        );
        start = hft::HFTTimer::get_timestamp_ns();
        auto risk_result = g_risk_framework->perform_comprehensive_check(risk_request);
        end = hft::HFTTimer::get_timestamp_ns();
        risk_latencies.push_back(end - start);
        
        g_quote_manager->cancel_quotes(quote_request.instrument);
    }
    
    // Calculate percentiles
    std::sort(decision_latencies.begin(), decision_latencies.end());
    std::sort(quote_latencies.begin(), quote_latencies.end());
    std::sort(risk_latencies.begin(), risk_latencies.end());
    
    auto decision_median = decision_latencies[decision_latencies.size() / 2];
    auto decision_p95 = decision_latencies[static_cast<size_t>(decision_latencies.size() * 0.95)];
    auto quote_median = quote_latencies[quote_latencies.size() / 2];
    auto quote_p95 = quote_latencies[static_cast<size_t>(quote_latencies.size() * 0.95)];
    auto risk_median = risk_latencies[risk_latencies.size() / 2];
    auto risk_p95 = risk_latencies[static_cast<size_t>(risk_latencies.size() * 0.95)];
    
    state.counters["StrategyDecision_Median_ns"] = decision_median;
    state.counters["StrategyDecision_P95_ns"] = decision_p95;
    state.counters["StrategyDecision_Target_Met"] = (decision_median <= 1200) ? 1 : 0;
    
    state.counters["QuoteProcessing_Median_ns"] = quote_median;
    state.counters["QuoteProcessing_P95_ns"] = quote_p95;
    state.counters["QuoteProcessing_Target_Met"] = (quote_median <= 300) ? 1 : 0;
    
    state.counters["RiskCheck_Median_ns"] = risk_median;
    state.counters["RiskCheck_P95_ns"] = risk_p95;
    state.counters["RiskCheck_Target_Met"] = (risk_median <= 450) ? 1 : 0;
    
    TeardownBenchmarks();
}
BENCHMARK(BM_PerformanceTargets_Validation)->Iterations(1);

BENCHMARK_MAIN();