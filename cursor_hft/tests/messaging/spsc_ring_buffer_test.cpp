#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <array>
#include <chrono>
#include <random>
#include <hft/messaging/spsc_ring_buffer.hpp>
#include <hft/timing/hft_timer.hpp>

using namespace hft;

// Test fixture for basic functionality
class SPSCRingBufferTest : public ::testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 1024;
    SPSCRingBuffer<int, BUFFER_SIZE> buffer;
};

// Test basic push/pop operations
TEST_F(SPSCRingBufferTest, BasicPushPop) {
    const int test_value = 42;
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 0);
    
    EXPECT_TRUE(buffer.try_push(test_value));
    EXPECT_FALSE(buffer.empty());
    EXPECT_EQ(buffer.size(), 1);
    
    int result = 0;
    EXPECT_TRUE(buffer.try_pop(result));
    EXPECT_EQ(result, test_value);
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
}

// Test buffer capacity
TEST_F(SPSCRingBufferTest, Capacity) {
    EXPECT_EQ(buffer.capacity(), BUFFER_SIZE - 1);
    
    // Fill buffer
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        EXPECT_TRUE(buffer.try_push(static_cast<int>(i)));
    }
    
    EXPECT_TRUE(buffer.full());
    EXPECT_FALSE(buffer.try_push(0)); // Should fail when full
    
    // Empty buffer
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        int value = 0;
        EXPECT_TRUE(buffer.try_pop(value));
        EXPECT_EQ(value, static_cast<int>(i));
    }
    
    EXPECT_TRUE(buffer.empty());
}

// Test batch operations
TEST_F(SPSCRingBufferTest, BatchOperations) {
    std::array<int, 100> input;
    std::array<int, 100> output;
    
    // Fill input array
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<int>(i);
    }
    
    // Test batch push
    size_t pushed = buffer.try_push_batch(input.begin(), input.end());
    EXPECT_EQ(pushed, input.size());
    EXPECT_EQ(buffer.size(), input.size());
    
    // Test batch pop
    size_t popped = buffer.try_pop_batch(output.begin(), output.end());
    EXPECT_EQ(popped, input.size());
    EXPECT_TRUE(buffer.empty());
    
    // Verify contents
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(input[i], output[i]);
    }
}

// Test concurrent producer/consumer
TEST_F(SPSCRingBufferTest, ConcurrentProducerConsumer) {
    static constexpr size_t NUM_ITEMS = 1000000;
    static constexpr size_t BATCH_SIZE = 100;
    
    std::atomic<bool> producer_done{false};
    std::atomic<size_t> items_produced{0};
    std::atomic<size_t> items_consumed{0};
    
    // Producer thread
    std::thread producer([&]() {
        std::array<int, BATCH_SIZE> batch;
        for (size_t i = 0; i < NUM_ITEMS; i += BATCH_SIZE) {
            // Fill batch
            for (size_t j = 0; j < BATCH_SIZE; ++j) {
                batch[j] = static_cast<int>(i + j);
            }
            
            // Try to push batch
            size_t pushed = 0;
            while (pushed < BATCH_SIZE) {
                pushed += buffer.try_push_batch(batch.begin() + pushed, batch.end());
                if (pushed < BATCH_SIZE) {
                    std::this_thread::yield();
                }
            }
            items_produced += BATCH_SIZE;
        }
        producer_done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        std::array<int, BATCH_SIZE> batch;
        while (!producer_done || !buffer.empty()) {
            size_t popped = buffer.try_pop_batch(batch.begin(), batch.end());
            if (popped > 0) {
                // Verify batch contents
                for (size_t i = 0; i < popped; ++i) {
                    EXPECT_EQ(batch[i], static_cast<int>(items_consumed + i));
                }
                items_consumed += popped;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(items_produced, NUM_ITEMS);
    EXPECT_EQ(items_consumed, NUM_ITEMS);
    EXPECT_TRUE(buffer.empty());
}

// Test performance characteristics
TEST_F(SPSCRingBufferTest, Performance) {
    static constexpr size_t NUM_ITERATIONS = 1000000;
    static constexpr size_t BATCH_SIZE = 100;
    
    // Measure single push/pop latency
    {
        auto start = HFTTimer::get_timestamp_ns();
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            EXPECT_TRUE(buffer.try_push(42));
            int value = 0;
            EXPECT_TRUE(buffer.try_pop(value));
            EXPECT_EQ(value, 42);
        }
        auto end = HFTTimer::get_timestamp_ns();
        auto latency = (end - start) / (NUM_ITERATIONS * 2); // Divide by 2 for push+pop
        EXPECT_LT(latency, 50); // Target: <50ns per operation
    }
    
    // Measure batch throughput
    {
        std::array<int, BATCH_SIZE> batch;
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            batch[i] = static_cast<int>(i);
        }
        
        auto start = HFTTimer::get_timestamp_ns();
        for (size_t i = 0; i < NUM_ITERATIONS / BATCH_SIZE; ++i) {
            EXPECT_EQ(buffer.try_push_batch(batch.begin(), batch.end()), BATCH_SIZE);
            EXPECT_EQ(buffer.try_pop_batch(batch.begin(), batch.end()), BATCH_SIZE);
        }
        auto end = HFTTimer::get_timestamp_ns();
        auto throughput = (NUM_ITERATIONS * 1000000000ULL) / (end - start);
        EXPECT_GT(throughput, 1000000); // Target: >1M ops/sec
    }
}

// Test with non-trivial types
struct TestMessage {
    int id;
    double price;
    char symbol[8];
    
    bool operator==(const TestMessage& other) const {
        return id == other.id && 
               price == other.price && 
               std::memcmp(symbol, other.symbol, sizeof(symbol)) == 0;
    }
};

TEST_F(SPSCRingBufferTest, NonTrivialType) {
    SPSCRingBuffer<TestMessage, 1024> msg_buffer;
    
    TestMessage msg{42, 123.45, "AAPL"};
    EXPECT_TRUE(msg_buffer.try_push(msg));
    
    TestMessage result{};
    EXPECT_TRUE(msg_buffer.try_pop(result));
    EXPECT_EQ(result, msg);
}

// Test error cases
TEST_F(SPSCRingBufferTest, ErrorCases) {
    // Try to pop from empty buffer
    int value = 0;
    EXPECT_FALSE(buffer.try_pop(value));
    
    // Fill buffer
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        EXPECT_TRUE(buffer.try_push(42));
    }
    
    // Try to push to full buffer
    EXPECT_FALSE(buffer.try_push(42));
    
    // Try batch operations with invalid iterators
    std::vector<int> empty_vec;
    EXPECT_EQ(buffer.try_push_batch(empty_vec.begin(), empty_vec.end()), 0);
    EXPECT_EQ(buffer.try_pop_batch(empty_vec.begin(), empty_vec.end()), 0);
} 