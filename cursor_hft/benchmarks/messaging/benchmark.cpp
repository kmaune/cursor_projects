#include <benchmark/benchmark.h>
#include <array>
#include <random>
#include <thread>
#include <chrono>
#include "../messaging/message.hpp"
#include "../messaging/ring_buffer.hpp"
#include "../messaging/message_dispatcher.hpp"
#include "../messaging/event_loop.hpp"
#include "../messaging/market_messages.hpp"
#include "hft/timing/hft_timer.hpp"

using namespace hft;

// Benchmark SPSC ring buffer
static void BM_SPSCRingBuffer(benchmark::State& state) {
    SPSCRingBuffer<MarketDataMessage, 1024> buffer;
    MarketDataMessage msg;
    msg.type = MarketDataMessage::Type::TICK;
    msg.price = 100;
    msg.quantity = 1000;
    msg.sequence = 0;

    for (auto _ : state) {
        if (buffer.try_push(msg)) {
            benchmark::DoNotOptimize(msg);
            buffer.try_pop(msg);
            benchmark::DoNotOptimize(msg);
        }
    }
}
BENCHMARK(BM_SPSCRingBuffer);

// Benchmark MPSC ring buffer
static void BM_MPSCRingBuffer(benchmark::State& state) {
    MPSCRingBuffer<MarketDataMessage, 1024> buffer;
    MarketDataMessage msg;
    msg.type = MarketDataMessage::Type::TICK;
    msg.price = 100;
    msg.quantity = 1000;
    msg.sequence = 0;

    for (auto _ : state) {
        if (buffer.try_push(msg)) {
            benchmark::DoNotOptimize(msg);
            buffer.try_pop(msg);
            benchmark::DoNotOptimize(msg);
        }
    }
}
BENCHMARK(BM_MPSCRingBuffer);

// Benchmark batch processing
static void BM_BatchProcessing(benchmark::State& state) {
    SPSCRingBuffer<MarketDataMessage, 1024> buffer;
    std::array<MarketDataMessage, 32> batch;
    
    for (size_t i = 0; i < batch.size(); ++i) {
        batch[i].type = MarketDataMessage::Type::TICK;
        batch[i].price = 100 + i;
        batch[i].quantity = 1000;
        batch[i].sequence = i;
    }

    for (auto _ : state) {
        size_t pushed = buffer.try_push_batch(batch.begin(), batch.end());
        if (pushed > 0) {
            std::array<MarketDataMessage, 32> received;
            size_t popped = buffer.try_pop_batch(received.begin(), received.end());
            benchmark::DoNotOptimize(received);
        }
    }
}
BENCHMARK(BM_BatchProcessing);

// Benchmark priority queue
static void BM_PriorityQueue(benchmark::State& state) {
    PriorityMessageQueue<MarketDataMessage> queue;
    MarketDataMessage msg;
    msg.type = MarketDataMessage::Type::TICK;
    msg.price = 100;
    msg.quantity = 1000;
    msg.sequence = 0;

    // Set different priorities
    for (auto _ : state) {
        for (int i = 0; i < 5; ++i) {
            msg.set_priority(static_cast<MessagePriority>(i));
            if (queue.try_enqueue(msg)) {
                benchmark::DoNotOptimize(msg);
                queue.try_dequeue(msg);
                benchmark::DoNotOptimize(msg);
            }
        }
    }
}
BENCHMARK(BM_PriorityQueue);

// Benchmark message dispatch
static void BM_MessageDispatch(benchmark::State& state) {
    MessageDispatcher dispatcher;
    MarketDataMessage msg;
    msg.type = MarketDataMessage::Type::TICK;
    msg.price = 100;
    msg.quantity = 1000;
    msg.sequence = 0;

    class TestHandler : public MessageHandler<MarketDataMessage> {
    public:
        void handle_message(const MarketDataMessage& msg) noexcept override {
            benchmark::DoNotOptimize(msg);
        }
    };

    TestHandler handler;
    dispatcher.register_handler(&handler);

    for (auto _ : state) {
        dispatcher.dispatch(msg);
    }
}
BENCHMARK(BM_MessageDispatch);

// Benchmark end-to-end latency
static void BM_EndToEndLatency(benchmark::State& state) {
    EventLoop event_loop;
    MessageDispatcher dispatcher;
    PriorityMessageQueue<MarketDataMessage> queue;
    
    class TestHandler : public MessageHandler<MarketDataMessage> {
    public:
        void handle_message(const MarketDataMessage& msg) noexcept override {
            benchmark::DoNotOptimize(msg);
        }
    };

    TestHandler handler;
    dispatcher.register_handler(&handler);
    event_loop.set_dispatcher(&dispatcher);
    event_loop.add_queue(&queue);
    event_loop.start();

    MarketDataMessage msg;
    msg.type = MarketDataMessage::Type::TICK;
    msg.price = 100;
    msg.quantity = 1000;
    msg.sequence = 0;

    for (auto _ : state) {
        const uint64_t start = HFTTimer::get_timestamp_ns();
        queue.try_enqueue(msg);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        const uint64_t end = HFTTimer::get_timestamp_ns();
        state.SetIterationTime((end - start) / 1e9);
    }

    event_loop.stop();
}
BENCHMARK(BM_EndToEndLatency);

BENCHMARK_MAIN(); 