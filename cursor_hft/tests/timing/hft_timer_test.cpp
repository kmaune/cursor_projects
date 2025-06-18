#include "hft/timing/hft_timer.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <chrono>

namespace hft {
namespace test {

class HFTTimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure timer is initialized
        HFTTimer::get_timestamp_ns();
    }
};

TEST_F(HFTTimerTest, BasicTiming) {
    const auto start = HFTTimer::get_timestamp_ns();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto end = HFTTimer::get_timestamp_ns();
    
    EXPECT_GT(end - start, 1000000); // Should be > 1ms
    EXPECT_LT(end - start, 2000000); // Should be < 2ms
}

TEST_F(HFTTimerTest, ScopedTimer) {
    LatencyHistogram histogram;
    
    {
        HFTTimer::ScopedTimer timer(histogram, "test_scope");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    const auto stats = histogram.get_stats();
    EXPECT_EQ(stats.total_samples, 1);
    EXPECT_GT(stats.mean_latency, 1000000);
    EXPECT_LT(stats.mean_latency, 2000000);
}

TEST_F(HFTTimerTest, HighFrequencyMeasurements) {
    LatencyHistogram histogram;
    constexpr size_t NUM_MEASUREMENTS = 1000000;
    
    for (size_t i = 0; i < NUM_MEASUREMENTS; ++i) {
        const auto start = HFTTimer::get_cycles();
        const auto end = HFTTimer::get_cycles();
        histogram.record_latency(HFTTimer::cycles_to_ns(end - start));
    }
    
    const auto stats = histogram.get_stats();
    EXPECT_EQ(stats.total_samples, NUM_MEASUREMENTS);
    EXPECT_GT(stats.mean_latency, 0);
    EXPECT_LT(stats.mean_latency, 1000); // Should be < 1us
}

TEST_F(HFTTimerTest, ThreadSafety) {
    LatencyHistogram histogram;
    constexpr size_t NUM_THREADS = 4;
    constexpr size_t MEASUREMENTS_PER_THREAD = 100000;
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&histogram]() {
            for (size_t j = 0; j < MEASUREMENTS_PER_THREAD; ++j) {
                const auto start = HFTTimer::get_cycles();
                const auto end = HFTTimer::get_cycles();
                histogram.record_latency(HFTTimer::cycles_to_ns(end - start));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    const auto stats = histogram.get_stats();
    EXPECT_EQ(stats.total_samples, NUM_THREADS * MEASUREMENTS_PER_THREAD);
}

TEST_F(HFTTimerTest, StatisticalAnalysis) {
    LatencyHistogram histogram;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> d(1000, 100); // Mean 1us, std dev 100ns
    
    constexpr size_t NUM_MEASUREMENTS = 100000;
    for (size_t i = 0; i < NUM_MEASUREMENTS; ++i) {
        const auto latency = static_cast<HFTTimer::ns_t>(std::abs(d(gen)));
        histogram.record_latency(latency);
    }
    
    const auto stats = histogram.get_stats();
    EXPECT_EQ(stats.total_samples, NUM_MEASUREMENTS);
    EXPECT_NEAR(stats.mean_latency, 1000, 50); // Mean should be ~1us
    EXPECT_GT(stats.std_dev, 0);
    EXPECT_LT(stats.std_dev, 200); // Std dev should be reasonable
    
    // Check percentiles
    EXPECT_GT(stats.percentiles[0], 0); // 50th percentile
    EXPECT_GT(stats.percentiles[1], stats.percentiles[0]); // 90th > 50th
    EXPECT_GT(stats.percentiles[2], stats.percentiles[1]); // 95th > 90th
    EXPECT_GT(stats.percentiles[3], stats.percentiles[2]); // 99th > 95th
}

TEST_F(HFTTimerTest, ResetFunctionality) {
    LatencyHistogram histogram;
    
    // Record some measurements
    for (int i = 0; i < 1000; ++i) {
        histogram.record_latency(1000);
    }
    
    EXPECT_EQ(histogram.get_stats().total_samples, 1000);
    
    // Reset and verify
    histogram.reset();
    const auto stats = histogram.get_stats();
    EXPECT_EQ(stats.total_samples, 0);
    EXPECT_EQ(stats.min_latency, std::numeric_limits<HFTTimer::ns_t>::max());
    EXPECT_EQ(stats.max_latency, 0);
    EXPECT_DOUBLE_EQ(stats.mean_latency, 0);
    EXPECT_DOUBLE_EQ(stats.std_dev, 0);
}

} // namespace test
} // namespace hft 