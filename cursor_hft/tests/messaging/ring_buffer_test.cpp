#include <gtest/gtest.h>
#include <array>
#include <thread>
#include <chrono>
#include "../../include/messaging/ring_buffer.hpp"
#include "../../include/messaging/market_messages.hpp"

using namespace hft;

// Test SPSC ring buffer basic operations
TEST(RingBufferTest, SPSCBasicOperations) {
    SPSCRingBuffer<MarketDataMessage, 16> buffer;
    MarketDataMessage msg;
    msg.type = MarketDataMessage::Type::TICK;
    msg.price = 100;
    msg.quantity = 1000;
    msg.sequence = 0;

    // Test empty state
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 0);

    // Test push
    EXPECT_TRUE(buffer.try_push(msg));
    EXPECT_FALSE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 1);

    // Test pop
    MarketDataMessage received;
    EXPECT_TRUE(buffer.try_pop(received));
    EXPECT_EQ(received.price, msg.price);
    EXPECT_EQ(received.quantity, msg.quantity);
    EXPECT_EQ(received.sequence, msg.sequence);
    EXPECT_TRUE(buffer.empty());
}

// Test SPSC ring buffer batch operations
TEST(RingBufferTest, SPSCBatchOperations) {
    SPSCRingBuffer<MarketDataMessage, 16> buffer;
    std::array<MarketDataMessage, 8> batch;
    
    // Fill batch
    for (size_t i = 0; i < batch.size(); ++i) {
        batch[i].type = MarketDataMessage::Type::TICK;
        batch[i].price = 100 + i;
        batch[i].quantity = 1000;
        batch[i].sequence = i;
    }

    // Test batch push
    size_t pushed = buffer.try_push_batch(batch.begin(), batch.end());
    EXPECT_EQ(pushed, batch.size());
    EXPECT_EQ(buffer.size(), batch.size());

    // Test batch pop
    std::array<MarketDataMessage, 8> received;
    size_t popped = buffer.try_pop_batch(received.begin(), received.end());
    EXPECT_EQ(popped, batch.size());
    
    for (size_t i = 0; i < popped; ++i) {
        EXPECT_EQ(received[i].price, batch[i].price);
        EXPECT_EQ(received[i].quantity, batch[i].quantity);
        EXPECT_EQ(received[i].sequence, batch[i].sequence);
    }
}

// Test MPSC ring buffer concurrent operations
TEST(RingBufferTest, MPSCConcurrentOperations) {
    MPSCRingBuffer<MarketDataMessage, 16> buffer;
    const size_t num_producers = 4;
    const size_t messages_per_producer = 1000;
    std::atomic<size_t> total_received{0};

    // Consumer thread
    std::thread consumer([&]() {
        MarketDataMessage msg;
        while (total_received < num_producers * messages_per_producer) {
            if (buffer.try_pop(msg)) {
                ++total_received;
            }
        }
    });

    // Producer threads
    std::vector<std::thread> producers;
    for (size_t i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i]() {
            MarketDataMessage msg;
            msg.type = MarketDataMessage::Type::TICK;
            msg.price = 100 + i;
            msg.quantity = 1000;
            msg.sequence = 0;

            for (size_t j = 0; j < messages_per_producer; ++j) {
                while (!buffer.try_push(msg)) {
                    std::this_thread::yield();
                }
                msg.sequence++;
            }
        });
    }

    // Wait for all threads
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();

    EXPECT_EQ(total_received, num_producers * messages_per_producer);
}

// Test ring buffer overflow
TEST(RingBufferTest, OverflowHandling) {
    SPSCRingBuffer<MarketDataMessage, 4> buffer;
    MarketDataMessage msg;
    msg.type = MarketDataMessage::Type::TICK;
    msg.price = 100;
    msg.quantity = 1000;
    msg.sequence = 0;

    // Fill buffer
    for (size_t i = 0; i < buffer.capacity; ++i) {
        EXPECT_TRUE(buffer.try_push(msg));
    }

    // Try to push when full
    EXPECT_FALSE(buffer.try_push(msg));
    EXPECT_TRUE(buffer.full());
}

// Test ring buffer underflow
TEST(RingBufferTest, UnderflowHandling) {
    SPSCRingBuffer<MarketDataMessage, 4> buffer;
    MarketDataMessage msg;

    // Try to pop when empty
    EXPECT_FALSE(buffer.try_pop(msg));
    EXPECT_TRUE(buffer.empty());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 