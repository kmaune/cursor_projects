#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <random>
#include <algorithm>
#include "hft/trading/order_book.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::trading;
using namespace hft::market_data;

// Global test fixtures for benchmarks
class OrderBookBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Initialize object pools and ring buffer
        order_pool_ = std::make_unique<TreasuryOrderPool>();
        level_pool_ = std::make_unique<PriceLevelPool>();
        update_buffer_ = std::make_unique<OrderBookUpdateBuffer>();
        
        // Create order book with pool references
        order_book_ = std::make_unique<TreasuryOrderBook>(
            *order_pool_, *level_pool_, *update_buffer_);
        
        // Initialize sequence counter
        next_sequence_ = 1;
        next_order_id_ = 1;
        
        // Pre-generate random data for consistent benchmarking
        generate_test_data(10000);
    }

    void TearDown(const ::benchmark::State& state) override {
        order_book_->reset();
        order_pool_->reset();
        level_pool_->reset();
    }

protected:
    // Helper function to create test orders
    TreasuryOrder create_test_order(OrderSide side, double price_decimal, uint64_t quantity) {
        Price32nd price = Price32nd::from_decimal(price_decimal);
        return TreasuryOrder(next_order_id_++, TreasuryType::Note_10Y, side, 
                           OrderType::LIMIT, price, quantity, next_sequence_++);
    }
    
    // Pre-generate test data for consistent benchmarking
    void generate_test_data(size_t count) {
        test_orders_.clear();
        test_orders_.reserve(count);
        
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::uniform_real_distribution<> price_dist(98.0, 102.0);
        std::uniform_int_distribution<uint64_t> qty_dist(100000, 5000000);
        std::uniform_int_distribution<> side_dist(0, 1);
        
        for (size_t i = 0; i < count; ++i) {
            OrderSide side = (side_dist(gen) == 0) ? OrderSide::BID : OrderSide::ASK;
            double price = price_dist(gen);
            uint64_t quantity = qty_dist(gen);
            test_orders_.emplace_back(create_test_order(side, price, quantity));
        }
    }
    
    // Reset order book state between benchmark runs
    void reset_state() {
        order_book_->reset();
        next_order_id_ = 1;
        next_sequence_ = 1;
    }

    std::unique_ptr<TreasuryOrderPool> order_pool_;
    std::unique_ptr<PriceLevelPool> level_pool_;
    std::unique_ptr<OrderBookUpdateBuffer> update_buffer_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::vector<TreasuryOrder> test_orders_;
    uint64_t next_sequence_;
    uint64_t next_order_id_;
};

// Core operation benchmarks - targeting specific latency requirements

BENCHMARK_F(OrderBookBenchmarkFixture, AddOrderLatency)(benchmark::State& state) {
    size_t order_idx = 0;
    
    for (auto _ : state) {
        state.PauseTiming();
        if (order_idx >= test_orders_.size()) {
            reset_state();
            order_idx = 0;
        }
        const auto& order = test_orders_[order_idx++];
        state.ResumeTiming();
        
        // Measure single order addition (target: <200ns)
        auto start = hft::HFTTimer::get_cycles();
        benchmark::DoNotOptimize(order_book_->add_order(order));
        auto end = hft::HFTTimer::get_cycles();
        
        state.SetIterationTime(hft::HFTTimer::cycles_to_ns(end - start) / 1e9);
    }
    
    state.SetLabel("Target: <200ns per order addition");
    state.counters["Orders"] = order_book_->get_stats().total_orders;
    state.counters["BidLevels"] = order_book_->get_stats().total_bid_levels;
    state.counters["AskLevels"] = order_book_->get_stats().total_ask_levels;
}

BENCHMARK_F(OrderBookBenchmarkFixture, CancelOrderLatency)(benchmark::State& state) {
    // Pre-populate book with orders
    std::vector<uint64_t> order_ids;
    for (size_t i = 0; i < 1000 && i < test_orders_.size(); ++i) {
        if (order_book_->add_order(test_orders_[i])) {
            order_ids.push_back(test_orders_[i].order_id);
        }
    }
    
    size_t cancel_idx = 0;
    
    for (auto _ : state) {
        state.PauseTiming();
        if (cancel_idx >= order_ids.size()) {
            // Repopulate
            reset_state();
            order_ids.clear();
            for (size_t i = 0; i < 1000 && i < test_orders_.size(); ++i) {
                if (order_book_->add_order(test_orders_[i])) {
                    order_ids.push_back(test_orders_[i].order_id);
                }
            }
            cancel_idx = 0;
        }
        uint64_t order_id = order_ids[cancel_idx++];
        state.ResumeTiming();
        
        // Measure single order cancellation (target: <200ns)
        auto start = hft::HFTTimer::get_cycles();
        benchmark::DoNotOptimize(order_book_->cancel_order(order_id));
        auto end = hft::HFTTimer::get_cycles();
        
        state.SetIterationTime(hft::HFTTimer::cycles_to_ns(end - start) / 1e9);
    }
    
    state.SetLabel("Target: <200ns per order cancellation");
}

BENCHMARK_F(OrderBookBenchmarkFixture, BestBidOfferLatency)(benchmark::State& state) {
    // Pre-populate book
    for (size_t i = 0; i < 100 && i < test_orders_.size(); ++i) {
        order_book_->add_order(test_orders_[i]);
    }
    
    for (auto _ : state) {
        // Measure best bid/offer lookup (target: <50ns)
        auto start = hft::HFTTimer::get_cycles();
        benchmark::DoNotOptimize(order_book_->get_best_bid());
        benchmark::DoNotOptimize(order_book_->get_best_ask());
        auto end = hft::HFTTimer::get_cycles();
        
        state.SetIterationTime(hft::HFTTimer::cycles_to_ns(end - start) / 1e9);
    }
    
    state.SetLabel("Target: <50ns per best bid/offer lookup");
}

BENCHMARK_F(OrderBookBenchmarkFixture, ModifyOrderLatency)(benchmark::State& state) {
    // Pre-populate book
    std::vector<uint64_t> order_ids;
    for (size_t i = 0; i < 500 && i < test_orders_.size(); ++i) {
        if (order_book_->add_order(test_orders_[i])) {
            order_ids.push_back(test_orders_[i].order_id);
        }
    }
    
    size_t modify_idx = 0;
    std::mt19937 gen(42);
    std::uniform_real_distribution<> price_dist(98.0, 102.0);
    std::uniform_int_distribution<uint64_t> qty_dist(100000, 5000000);
    
    for (auto _ : state) {
        state.PauseTiming();
        if (modify_idx >= order_ids.size() || order_ids.empty()) {
            // Repopulate
            reset_state();
            order_ids.clear();
            for (size_t i = 0; i < 500 && i < test_orders_.size(); ++i) {
                if (order_book_->add_order(test_orders_[i])) {
                    order_ids.push_back(test_orders_[i].order_id);
                }
            }
            modify_idx = 0;
        }
        
        uint64_t order_id = order_ids[modify_idx++];
        Price32nd new_price = Price32nd::from_decimal(price_dist(gen));
        uint64_t new_qty = qty_dist(gen);
        state.ResumeTiming();
        
        // Measure order modification (target: component of <500ns total)
        auto start = hft::HFTTimer::get_cycles();
        benchmark::DoNotOptimize(order_book_->modify_order(order_id, new_price, new_qty));
        auto end = hft::HFTTimer::get_cycles();
        
        state.SetIterationTime(hft::HFTTimer::cycles_to_ns(end - start) / 1e9);
    }
    
    state.SetLabel("Target: <500ns per order modification");
}

BENCHMARK_F(OrderBookBenchmarkFixture, ProcessTradeLatency)(benchmark::State& state) {
    // Pre-populate book with multiple price levels
    for (size_t i = 0; i < 200 && i < test_orders_.size(); ++i) {
        order_book_->add_order(test_orders_[i]);
    }
    
    std::mt19937 gen(42);
    std::uniform_real_distribution<> price_dist(98.0, 102.0);
    std::uniform_int_distribution<uint64_t> qty_dist(100000, 1000000);
    std::uniform_int_distribution<> side_dist(0, 1);
    
    for (auto _ : state) {
        state.PauseTiming();
        Price32nd trade_price = Price32nd::from_decimal(price_dist(gen));
        uint64_t trade_qty = qty_dist(gen);
        OrderSide trade_side = (side_dist(gen) == 0) ? OrderSide::BID : OrderSide::ASK;
        state.ResumeTiming();
        
        // Measure trade processing (target: component of <500ns total)
        auto start = hft::HFTTimer::get_cycles();
        benchmark::DoNotOptimize(order_book_->process_trade(trade_price, trade_qty, trade_side));
        auto end = hft::HFTTimer::get_cycles();
        
        state.SetIterationTime(hft::HFTTimer::cycles_to_ns(end - start) / 1e9);
    }
    
    state.SetLabel("Target: <500ns per trade processing");
}

// Throughput benchmarks
BENCHMARK_F(OrderBookBenchmarkFixture, OrderAdditionThroughput)(benchmark::State& state) {
    size_t order_idx = 0;
    size_t successful_adds = 0;
    
    for (auto _ : state) {
        if (order_idx >= test_orders_.size()) {
            reset_state();
            order_idx = 0;
        }
        
        if (order_book_->add_order(test_orders_[order_idx++])) {
            ++successful_adds;
        }
    }
    
    state.counters["OrdersPerSecond"] = benchmark::Counter(
        successful_adds, benchmark::Counter::kIsRate);
    state.counters["TotalOrders"] = successful_adds;
    state.SetLabel("Sustained order addition rate");
}

BENCHMARK_F(OrderBookBenchmarkFixture, MixedOperationsThroughput)(benchmark::State& state) {
    std::mt19937 gen(42);
    std::uniform_int_distribution<> operation_dist(0, 3); // add, cancel, modify, query
    std::vector<uint64_t> active_orders;
    
    size_t operations = 0;
    size_t order_idx = 0;
    
    for (auto _ : state) {
        int operation = operation_dist(gen);
        
        if (operation == 0 || active_orders.empty()) {
            // Add order
            if (order_idx < test_orders_.size()) {
                if (order_book_->add_order(test_orders_[order_idx])) {
                    active_orders.push_back(test_orders_[order_idx].order_id);
                }
                ++order_idx;
            }
        } else if (operation == 1 && !active_orders.empty()) {
            // Cancel order
            size_t idx = gen() % active_orders.size();
            uint64_t order_id = active_orders[idx];
            if (order_book_->cancel_order(order_id)) {
                active_orders.erase(active_orders.begin() + idx);
            }
        } else if (operation == 2 && !active_orders.empty()) {
            // Modify order (this might fail if order was cancelled)
            uint64_t order_id = active_orders[gen() % active_orders.size()];
            Price32nd new_price = Price32nd::from_decimal(98.0 + (gen() % 400) * 0.01);
            order_book_->modify_order(order_id, new_price, 1000000);
        } else {
            // Query operations
            benchmark::DoNotOptimize(order_book_->get_best_bid());
            benchmark::DoNotOptimize(order_book_->get_best_ask());
        }
        
        ++operations;
        
        // Reset periodically to avoid memory exhaustion
        if (operations % 10000 == 0) {
            reset_state();
            active_orders.clear();
            order_idx = 0;
        }
    }
    
    state.counters["OperationsPerSecond"] = benchmark::Counter(
        operations, benchmark::Counter::kIsRate);
    state.SetLabel("Mixed operations throughput");
}

// Memory and cache behavior benchmarks
BENCHMARK_F(OrderBookBenchmarkFixture, MarketDepthRetrieval)(benchmark::State& state) {
    // Create a deep book with many levels
    std::vector<TreasuryOrder> depth_orders;
    for (int i = 0; i < 50; ++i) {
        double bid_price = 99.0 - i * 0.03125;
        double ask_price = 100.0 + i * 0.03125;
        
        depth_orders.emplace_back(create_test_order(OrderSide::BID, bid_price, 1000000));
        depth_orders.emplace_back(create_test_order(OrderSide::ASK, ask_price, 1000000));
    }
    
    for (const auto& order : depth_orders) {
        order_book_->add_order(order);
    }
    
    for (auto _ : state) {
        // Measure market depth retrieval
        benchmark::DoNotOptimize(order_book_->get_market_depth(OrderSide::BID, 10));
        benchmark::DoNotOptimize(order_book_->get_market_depth(OrderSide::ASK, 10));
    }
    
    state.SetLabel("Market depth retrieval (10 levels each side)");
}

BENCHMARK_F(OrderBookBenchmarkFixture, ObjectPoolEfficiency)(benchmark::State& state) {
    size_t pool_acquisitions = 0;
    
    for (auto _ : state) {
        state.PauseTiming();
        reset_state();
        state.ResumeTiming();
        
        // Measure time to fill and empty the book (tests pool efficiency)
        auto start = hft::HFTTimer::get_cycles();
        
        std::vector<uint64_t> added_orders;
        for (size_t i = 0; i < 100 && i < test_orders_.size(); ++i) {
            if (order_book_->add_order(test_orders_[i])) {
                added_orders.push_back(test_orders_[i].order_id);
                ++pool_acquisitions;
            }
        }
        
        for (uint64_t order_id : added_orders) {
            order_book_->cancel_order(order_id);
        }
        
        auto end = hft::HFTTimer::get_cycles();
        state.SetIterationTime(hft::HFTTimer::cycles_to_ns(end - start) / 1e9);
    }
    
    state.counters["PoolAcquisitions"] = pool_acquisitions;
    state.SetLabel("Object pool allocation/deallocation efficiency");
}

// Treasury-specific benchmarks
BENCHMARK_F(OrderBookBenchmarkFixture, Treasury32ndPriceComparison)(benchmark::State& state) {
    // Test price comparison performance with 32nd fractional pricing
    std::vector<Price32nd> prices;
    for (int i = 0; i < 1000; ++i) {
        double decimal = 99.0 + (i * 0.03125 / 32.0); // Very fine price increments
        prices.push_back(Price32nd::from_decimal(decimal));
    }
    
    size_t comparisons = 0;
    
    for (auto _ : state) {
        for (size_t i = 0; i < prices.size() - 1; ++i) {
            // Simulate price comparisons used in order book operations
            benchmark::DoNotOptimize(prices[i].to_decimal() < prices[i + 1].to_decimal());
            ++comparisons;
        }
    }
    
    state.counters["ComparisonsPerSecond"] = benchmark::Counter(
        comparisons, benchmark::Counter::kIsRate);
    state.SetLabel("32nd fractional price comparisons");
}

// Latency distribution benchmarks
BENCHMARK_F(OrderBookBenchmarkFixture, LatencyDistributionAnalysis)(benchmark::State& state) {
    std::vector<uint64_t> latencies;
    latencies.reserve(10000);
    
    size_t order_idx = 0;
    
    for (auto _ : state) {
        state.PauseTiming();
        if (order_idx >= test_orders_.size()) {
            // Analyze collected latencies
            if (!latencies.empty()) {
                std::sort(latencies.begin(), latencies.end());
                
                uint64_t min_lat = latencies.front();
                uint64_t max_lat = latencies.back();
                uint64_t p50 = latencies[latencies.size() / 2];
                uint64_t p90 = latencies[latencies.size() * 9 / 10];
                uint64_t p99 = latencies[latencies.size() * 99 / 100];
                
                state.counters["MinLatency_ns"] = min_lat;
                state.counters["MaxLatency_ns"] = max_lat;
                state.counters["P50Latency_ns"] = p50;
                state.counters["P90Latency_ns"] = p90;
                state.counters["P99Latency_ns"] = p99;
            }
            
            reset_state();
            latencies.clear();
            order_idx = 0;
        }
        
        const auto& order = test_orders_[order_idx++];
        state.ResumeTiming();
        
        // Collect individual operation latencies
        auto start = hft::HFTTimer::get_cycles();
        order_book_->add_order(order);
        auto end = hft::HFTTimer::get_cycles();
        
        latencies.push_back(hft::HFTTimer::cycles_to_ns(end - start));
    }
    
    state.SetLabel("Order addition latency distribution");
}

// Load testing benchmarks
BENCHMARK_F(OrderBookBenchmarkFixture, HighLoadSustainedOperations)(benchmark::State& state) {
    std::mt19937 gen(42);
    std::uniform_int_distribution<> operation_dist(0, 2);
    std::vector<uint64_t> active_orders;
    
    size_t total_ops = 0;
    size_t order_idx = 0;
    
    // Run sustained high-load operations
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto _ : state) {
        // Perform batch of operations
        for (int batch = 0; batch < 100; ++batch) {
            int op = operation_dist(gen);
            
            if (op == 0 || active_orders.size() < 1000) {
                // Add order
                if (order_idx < test_orders_.size()) {
                    if (order_book_->add_order(test_orders_[order_idx])) {
                        active_orders.push_back(test_orders_[order_idx].order_id);
                    }
                    ++order_idx;
                }
            } else if (op == 1 && !active_orders.empty()) {
                // Cancel order
                size_t idx = gen() % active_orders.size();
                uint64_t order_id = active_orders[idx];
                if (order_book_->cancel_order(order_id)) {
                    active_orders.erase(active_orders.begin() + idx);
                }
            } else {
                // Query
                benchmark::DoNotOptimize(order_book_->get_best_bid());
                benchmark::DoNotOptimize(order_book_->get_best_ask());
            }
            ++total_ops;
        }
        
        // Reset periodically
        if (total_ops % 50000 == 0) {
            reset_state();
            active_orders.clear();
            order_idx = 0;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    state.counters["TotalOperations"] = total_ops;
    state.counters["ActiveOrders"] = active_orders.size();
    state.counters["OperationsPerSecond"] = benchmark::Counter(total_ops, benchmark::Counter::kIsRate);
    state.SetLabel("High-load sustained operations");
}

// Register all benchmarks with appropriate iterations and timing
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, AddOrderLatency)
    ->UseManualTime()->Iterations(10000)->Unit(benchmark::kNanosecond);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, CancelOrderLatency)
    ->UseManualTime()->Iterations(10000)->Unit(benchmark::kNanosecond);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, BestBidOfferLatency)
    ->UseManualTime()->Iterations(100000)->Unit(benchmark::kNanosecond);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, ModifyOrderLatency)
    ->UseManualTime()->Iterations(5000)->Unit(benchmark::kNanosecond);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, ProcessTradeLatency)
    ->UseManualTime()->Iterations(5000)->Unit(benchmark::kNanosecond);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, OrderAdditionThroughput)
    ->Iterations(50000);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, MixedOperationsThroughput)
    ->Iterations(100000);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, MarketDepthRetrieval)
    ->Iterations(10000);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, ObjectPoolEfficiency)
    ->UseManualTime()->Iterations(1000)->Unit(benchmark::kNanosecond);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, Treasury32ndPriceComparison)
    ->Iterations(10000);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, LatencyDistributionAnalysis)
    ->Iterations(1000);
    
BENCHMARK_REGISTER_F(OrderBookBenchmarkFixture, HighLoadSustainedOperations)
    ->Iterations(100);

BENCHMARK_MAIN();