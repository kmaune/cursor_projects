#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <random>
#include <chrono>
#include <cstring>
#include "hft/market_data/feed_handler.hpp"
#include "hft/trading/order_book.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft::market_data;
using namespace hft::trading;

/**
 * @brief End-to-end system integration benchmark
 * 
 * This benchmark measures the complete tick-to-trade pipeline:
 * 1. Raw market message ingestion (Feed Handler)
 * 2. Message parsing and validation
 * 3. Order book updates
 * 4. Trade decision logic (simulated)
 * 5. Order placement response
 * 
 * Target: <15 microseconds end-to-end
 */
class EndToEndBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // Initialize object pools
        order_pool_ = std::make_unique<TreasuryOrderPool>();
        level_pool_ = std::make_unique<PriceLevelPool>();
        update_buffer_ = std::make_unique<OrderBookUpdateBuffer>();
        
        // Initialize core components
        feed_handler_ = std::make_unique<TreasuryFeedHandler>();
        order_book_ = std::make_unique<TreasuryOrderBook>(
            *order_pool_, *level_pool_, *update_buffer_);
        
        // Pre-generate test data for consistent benchmarking
        generate_market_data(1000);
        next_order_id_ = 1;
    }

    void TearDown(const ::benchmark::State& state) override {
        order_book_->reset();
        order_pool_->reset();
        level_pool_->reset();
        feed_handler_->reset_stats();
    }

protected:
    // Generate realistic market data messages
    void generate_market_data(size_t count) {
        test_messages_.clear();
        test_messages_.reserve(count);
        
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::uniform_real_distribution<> price_dist(99.0, 101.0);
        std::uniform_int_distribution<uint64_t> qty_dist(1000000, 10000000);
        std::uniform_int_distribution<> instrument_dist(1, 6);
        
        for (size_t i = 0; i < count; ++i) {
            test_messages_.emplace_back(make_raw_message(
                i + 1, MessageType::Tick, instrument_dist(gen), 
                price_dist(gen), qty_dist(gen)));
        }
    }
    
    // Create a realistic raw market message
    RawMarketMessage make_raw_message(uint64_t seq, MessageType type, 
                                    uint32_t instr_id, double price, uint64_t size) {
        RawMarketMessage msg{};
        msg.sequence_number = seq;
        msg.timestamp_exchange_ns = hft::HFTTimer::get_timestamp_ns();
        msg.message_type = static_cast<uint32_t>(type);
        msg.instrument_id = instr_id;
        
        // Bid price, ask price, bid size, ask size
        double ask_price = price + 0.03125; // 1/32nd spread
        std::memcpy(msg.raw_data, &price, sizeof(double));
        std::memcpy(msg.raw_data + 8, &ask_price, sizeof(double));
        std::memcpy(msg.raw_data + 16, &size, sizeof(uint64_t));
        std::memcpy(msg.raw_data + 24, &size, sizeof(uint64_t));
        
        // Compute checksum
        uint16_t sum = 0;
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&msg);
        for (size_t i = 0; i < offsetof(RawMarketMessage, checksum); ++i) {
            sum ^= ptr[i];
        }
        msg.checksum = sum;
        
        return msg;
    }
    
    // Simulate trading decision logic
    bool should_trade(const TreasuryTick& tick) {
        // Simple spread-based trading logic
        double bid_decimal = tick.bid_price.to_decimal();
        double ask_decimal = tick.ask_price.to_decimal();
        double spread = ask_decimal - bid_decimal;
        
        // Trade if spread is attractive (> 2/32nds = 0.0625)
        return spread > 0.0625 && tick.bid_size > 1000000 && tick.ask_size > 1000000;
    }
    
    // Create orders based on market data
    TreasuryOrder create_responsive_order(const TreasuryTick& tick) {
        // Create a bid order slightly inside the market
        Price32nd order_price = Price32nd::from_decimal(tick.bid_price.to_decimal() + 0.015625);
        uint64_t order_qty = std::min(tick.bid_size / 2, static_cast<uint64_t>(5000000));
        
        uint64_t order_id = next_order_id_++;
        return TreasuryOrder(order_id, tick.instrument_type, OrderSide::BID,
                           OrderType::LIMIT, order_price, order_qty, order_id);
    }

    std::unique_ptr<TreasuryOrderPool> order_pool_;
    std::unique_ptr<PriceLevelPool> level_pool_;
    std::unique_ptr<OrderBookUpdateBuffer> update_buffer_;
    std::unique_ptr<TreasuryFeedHandler> feed_handler_;
    std::unique_ptr<TreasuryOrderBook> order_book_;
    std::vector<RawMarketMessage> test_messages_;
    uint64_t next_order_id_;
};

// Core end-to-end tick-to-trade latency benchmark
BENCHMARK_F(EndToEndBenchmarkFixture, TickToTradeLatency)(benchmark::State& state) {
    size_t processed_messages = 0;
    size_t generated_orders = 0;
    std::vector<uint64_t> latencies;
    latencies.reserve(1000);
    
    for (auto _ : state) {
        state.PauseTiming();
        
        // Reset for clean measurement
        if (processed_messages >= test_messages_.size()) {
            order_book_->reset();
            feed_handler_->reset_stats();
            processed_messages = 0;
        }
        
        const auto& raw_msg = test_messages_[processed_messages++];
        state.ResumeTiming();
        
        // Start end-to-end timing
        auto start_cycles = hft::HFTTimer::get_cycles();
        
        // 1. Feed Handler Processing (market data ingestion)
        size_t parsed = feed_handler_->process_messages(&raw_msg, 1);
        
        if (parsed > 0) {
            // 2. Extract parsed market data
            TreasuryTick tick;
            size_t retrieved = feed_handler_->get_parsed_ticks(&tick, 1);
            
            if (retrieved > 0) {
                // 3. Trading Decision Logic
                if (should_trade(tick)) {
                    // 4. Order Book Update (order placement)
                    TreasuryOrder order = create_responsive_order(tick);
                    bool order_success = order_book_->add_order(order);
                    
                    if (order_success) {
                        ++generated_orders;
                    }
                }
            }
        }
        
        // End timing
        auto end_cycles = hft::HFTTimer::get_cycles();
        uint64_t latency_ns = hft::HFTTimer::cycles_to_ns(end_cycles - start_cycles);
        latencies.push_back(latency_ns);
        
        state.SetIterationTime(latency_ns / 1e9);
    }
    
    // Calculate statistics
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        uint64_t min_lat = latencies.front();
        uint64_t max_lat = latencies.back();
        uint64_t median = latencies[latencies.size() / 2];
        uint64_t p95 = latencies[latencies.size() * 95 / 100];
        uint64_t p99 = latencies[latencies.size() * 99 / 100];
        
        state.counters["MinLatency_ns"] = min_lat;
        state.counters["MaxLatency_ns"] = max_lat;
        state.counters["MedianLatency_ns"] = median;
        state.counters["P95Latency_ns"] = p95;
        state.counters["P99Latency_ns"] = p99;
        state.counters["OrdersGenerated"] = generated_orders;
        state.counters["MessagesProcessed"] = processed_messages;
    }
    
    state.SetLabel("Target: <15μs tick-to-trade");
}

// Component interaction overhead analysis
BENCHMARK_F(EndToEndBenchmarkFixture, ComponentInteractionOverhead)(benchmark::State& state) {
    size_t batch_size = 10;
    std::vector<uint64_t> feed_handler_times;
    std::vector<uint64_t> order_book_times;
    std::vector<uint64_t> integration_times;
    
    for (auto _ : state) {
        state.PauseTiming();
        
        // Process a batch of messages
        auto batch_start = test_messages_.begin();
        auto batch_end = batch_start + std::min(batch_size, test_messages_.size());
        
        state.ResumeTiming();
        
        // Measure feed handler processing time
        auto fh_start = hft::HFTTimer::get_cycles();
        size_t processed = feed_handler_->process_messages(&(*batch_start), batch_size);
        auto fh_end = hft::HFTTimer::get_cycles();
        feed_handler_times.push_back(hft::HFTTimer::cycles_to_ns(fh_end - fh_start));
        
        // Measure order book interaction time
        std::vector<TreasuryTick> ticks(batch_size);
        size_t retrieved = feed_handler_->get_parsed_ticks(ticks.data(), batch_size);
        
        auto ob_start = hft::HFTTimer::get_cycles();
        size_t orders_placed = 0;
        for (size_t i = 0; i < retrieved; ++i) {
            if (should_trade(ticks[i])) {
                TreasuryOrder order = create_responsive_order(ticks[i]);
                if (order_book_->add_order(order)) {
                    ++orders_placed;
                }
            }
        }
        auto ob_end = hft::HFTTimer::get_cycles();
        order_book_times.push_back(hft::HFTTimer::cycles_to_ns(ob_end - ob_start));
        
        // Measure ring buffer interaction time
        auto ring_start = hft::HFTTimer::get_cycles();
        std::vector<OrderBookUpdate> updates(batch_size);
        size_t updates_retrieved = update_buffer_->try_pop_batch(updates.begin(), updates.end());
        auto ring_end = hft::HFTTimer::get_cycles();
        integration_times.push_back(hft::HFTTimer::cycles_to_ns(ring_end - ring_start));
        
        state.counters["OrdersPlaced"] = orders_placed;
        state.counters["UpdatesRetrieved"] = updates_retrieved;
    }
    
    // Report component timing breakdown
    if (!feed_handler_times.empty()) {
        uint64_t avg_fh = std::accumulate(feed_handler_times.begin(), feed_handler_times.end(), 0ULL) / feed_handler_times.size();
        uint64_t avg_ob = std::accumulate(order_book_times.begin(), order_book_times.end(), 0ULL) / order_book_times.size();
        uint64_t avg_ring = std::accumulate(integration_times.begin(), integration_times.end(), 0ULL) / integration_times.size();
        
        state.counters["AvgFeedHandler_ns"] = avg_fh;
        state.counters["AvgOrderBook_ns"] = avg_ob;
        state.counters["AvgRingBuffer_ns"] = avg_ring;
        state.counters["TotalOverhead_ns"] = avg_fh + avg_ob + avg_ring;
    }
    
    state.SetLabel("Component interaction analysis");
}

// High-throughput sustained load test
BENCHMARK_F(EndToEndBenchmarkFixture, HighThroughputSustainedLoad)(benchmark::State& state) {
    size_t burst_size = 100; // Process messages in bursts
    size_t total_processed = 0;
    size_t total_orders = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto _ : state) {
        // Process a burst of messages
        size_t batch_start_idx = total_processed % test_messages_.size();
        size_t actual_burst = std::min(burst_size, test_messages_.size() - batch_start_idx);
        
        const RawMarketMessage* batch_start = &test_messages_[batch_start_idx];
        
        // Process the burst
        size_t processed = feed_handler_->process_messages(batch_start, actual_burst);
        total_processed += processed;
        
        // Extract and process ticks
        std::vector<TreasuryTick> ticks(actual_burst);
        size_t retrieved = feed_handler_->get_parsed_ticks(ticks.data(), actual_burst);
        
        // Generate orders
        for (size_t i = 0; i < retrieved; ++i) {
            if (should_trade(ticks[i])) {
                TreasuryOrder order = create_responsive_order(ticks[i]);
                if (order_book_->add_order(order)) {
                    ++total_orders;
                }
            }
        }
        
        // Reset periodically to prevent memory exhaustion
        if (total_processed % 10000 == 0) {
            order_book_->reset();
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    double messages_per_sec = (double)total_processed / (duration.count() / 1000.0);
    double orders_per_sec = (double)total_orders / (duration.count() / 1000.0);
    
    state.counters["MessagesPerSec"] = messages_per_sec;
    state.counters["OrdersPerSec"] = orders_per_sec;
    state.counters["TotalMessages"] = total_processed;
    state.counters["TotalOrders"] = total_orders;
    state.counters["OrderRate_%"] = (total_orders > 0) ? (double)total_orders / total_processed * 100.0 : 0.0;
    
    state.SetLabel("High-throughput sustained load");
}

// Memory allocation and cache behavior analysis
BENCHMARK_F(EndToEndBenchmarkFixture, MemoryAllocationAnalysis)(benchmark::State& state) {
    size_t initial_order_pool = order_pool_->available();
    size_t initial_level_pool = level_pool_->available();
    
    for (auto _ : state) {
        state.PauseTiming();
        
        // Reset pools to measure allocation behavior
        order_book_->reset();
        order_pool_->reset();
        level_pool_->reset();
        
        size_t start_orders = order_pool_->available();
        size_t start_levels = level_pool_->available();
        
        state.ResumeTiming();
        
        // Process messages and measure pool utilization
        for (size_t i = 0; i < 100 && i < test_messages_.size(); ++i) {
            feed_handler_->process_messages(&test_messages_[i], 1);
            
            TreasuryTick tick;
            if (feed_handler_->get_parsed_ticks(&tick, 1) > 0) {
                if (should_trade(tick)) {
                    TreasuryOrder order = create_responsive_order(tick);
                    order_book_->add_order(order);
                }
            }
        }
        
        size_t end_orders = order_pool_->available();
        size_t end_levels = level_pool_->available();
        
        state.counters["OrdersAllocated"] = start_orders - end_orders;
        state.counters["LevelsAllocated"] = start_levels - end_levels;
        state.counters["PoolEfficiency_%"] = ((double)(start_orders - end_orders) / start_orders) * 100.0;
    }
    
    state.SetLabel("Zero-allocation validation");
}

// Register benchmarks
BENCHMARK_REGISTER_F(EndToEndBenchmarkFixture, TickToTradeLatency)
    ->UseManualTime()->Iterations(1000)->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(EndToEndBenchmarkFixture, ComponentInteractionOverhead)
    ->Iterations(1000);

BENCHMARK_REGISTER_F(EndToEndBenchmarkFixture, HighThroughputSustainedLoad)
    ->Iterations(1000);

BENCHMARK_REGISTER_F(EndToEndBenchmarkFixture, MemoryAllocationAnalysis)
    ->Iterations(100);

BENCHMARK_MAIN();