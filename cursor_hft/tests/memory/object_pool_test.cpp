#include <gtest/gtest.h>
#include "hft/memory/object_pool.hpp"
#include "hft/timing/hft_timer.hpp"
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <random>
#include <type_traits>

namespace hft {
namespace {

// Trivially destructible struct for testing
struct alignas(64) TestStruct {
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
};

constexpr size_t POOL_SIZE = 128;
using Pool = ObjectPool<TestStruct, POOL_SIZE, false>;
using TimedPool = ObjectPool<TestStruct, POOL_SIZE, true>;

TEST(ObjectPoolTest, BasicAcquireRelease) {
    Pool pool;
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), POOL_SIZE);
    std::vector<TestStruct*> ptrs;
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        auto* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);
        ptrs.push_back(obj);
    }
    EXPECT_EQ(pool.size(), POOL_SIZE);
    EXPECT_EQ(pool.available(), 0u);
    // Pool exhausted
    EXPECT_EQ(pool.acquire(), nullptr);
    // Release all
    for (auto* obj : ptrs) {
        pool.release(obj);
    }
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), POOL_SIZE);
}

TEST(ObjectPoolTest, TimedBasicAcquireRelease) {
    TimedPool pool;
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), POOL_SIZE);
    std::vector<TestStruct*> ptrs;
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        auto* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);
        ptrs.push_back(obj);
    }
    EXPECT_EQ(pool.size(), POOL_SIZE);
    EXPECT_EQ(pool.available(), 0u);
    // Pool exhausted
    EXPECT_EQ(pool.acquire(), nullptr);
    // Release all
    for (auto* obj : ptrs) {
        pool.release(obj);
    }
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), POOL_SIZE);
}

TEST(ObjectPoolTest, ResetReturnsAllObjects) {
    Pool pool;
    std::vector<TestStruct*> ptrs;
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        ptrs.push_back(pool.acquire());
    }
    pool.reset();
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), POOL_SIZE);
    // All objects can be reacquired
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        ASSERT_NE(pool.acquire(), nullptr);
    }
}

TEST(ObjectPoolTest, TimedResetReturnsAllObjects) {
    TimedPool pool;
    std::vector<TestStruct*> ptrs;
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        ptrs.push_back(pool.acquire());
    }
    pool.reset();
    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.available(), POOL_SIZE);
    // All objects can be reacquired
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        ASSERT_NE(pool.acquire(), nullptr);
    }
}

TEST(ObjectPoolTest, MemoryValidation) {
    Pool pool;
    EXPECT_TRUE(pool.validate_memory());
    TimedPool tpool;
    EXPECT_TRUE(tpool.validate_memory());
    // Static asserts for alignment
    static_assert(alignof(Pool) == 64, "Pool not cache-line aligned");
    static_assert(alignof(TimedPool) == 64, "TimedPool not cache-line aligned");
    static_assert(alignof(TestStruct) == 64, "TestStruct not cache-line aligned");
}

TEST(ObjectPoolTest, EdgeCases) {
    Pool pool;
    // Release nullptr (should assert in debug, skip in release)
    // pool.release(nullptr); // Uncomment to test debug assert
    // Double release (should assert)
    auto* obj = pool.acquire();
    pool.release(obj);
    // pool.release(obj); // Uncomment to test debug assert
}

TEST(ObjectPoolTest, AllocationLatency) {
    TimedPool pool;
    std::vector<TestStruct*> ptrs;
    hft::HFTTimer::ns_t total_ns = 0;
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        auto* obj = pool.acquire();
        ASSERT_NE(obj, nullptr);
        total_ns += pool.last_alloc_ns();
        ptrs.push_back(obj);
    }
    double avg_ns = static_cast<double>(total_ns) / POOL_SIZE;
    // Expect sub-microsecond average allocation
    EXPECT_LT(avg_ns, 1000.0);
    for (auto* obj : ptrs) pool.release(obj);
}

TEST(ObjectPoolTest, BatchAllocationPrefetch) {
    Pool pool;
    std::array<TestStruct*, POOL_SIZE> ptrs{};
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        ptrs[i] = pool.acquire();
    }
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        pool.release(ptrs[i]);
    }
    EXPECT_EQ(pool.available(), POOL_SIZE);
}

// Performance benchmark: compare timed vs non-timed acquire()
TEST(ObjectPoolBenchmark, TimedVsNonTimedAcquire) {
    constexpr size_t N = 1000000;
    Pool pool;
    TimedPool tpool;
    std::vector<TestStruct*> ptrs(POOL_SIZE);
    hft::HFTTimer::ns_t alloc_total = 0, free_total = 0;
    hft::HFTTimer::ns_t talloc_total = 0, tfree_total = 0;
    // Non-timed
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < POOL_SIZE; ++j) {
            auto start = hft::HFTTimer::get_cycles();
            ptrs[j] = pool.acquire();
            alloc_total += hft::HFTTimer::cycles_to_ns(hft::HFTTimer::get_cycles() - start);
        }
        for (size_t j = 0; j < POOL_SIZE; ++j) {
            auto start = hft::HFTTimer::get_cycles();
            pool.release(ptrs[j]);
            free_total += hft::HFTTimer::cycles_to_ns(hft::HFTTimer::get_cycles() - start);
        }
    }
    // Timed
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < POOL_SIZE; ++j) {
            auto start = hft::HFTTimer::get_cycles();
            ptrs[j] = tpool.acquire();
            talloc_total += hft::HFTTimer::cycles_to_ns(hft::HFTTimer::get_cycles() - start);
        }
        for (size_t j = 0; j < POOL_SIZE; ++j) {
            auto start = hft::HFTTimer::get_cycles();
            tpool.release(ptrs[j]);
            tfree_total += hft::HFTTimer::cycles_to_ns(hft::HFTTimer::get_cycles() - start);
        }
    }
    double avg_alloc = static_cast<double>(alloc_total) / (N * POOL_SIZE);
    double avg_free = static_cast<double>(free_total) / (N * POOL_SIZE);
    double tavg_alloc = static_cast<double>(talloc_total) / (N * POOL_SIZE);
    double tavg_free = static_cast<double>(tfree_total) / (N * POOL_SIZE);
    // Print for manual inspection
    printf("Non-timed: Avg alloc: %.2f ns, Avg free: %.2f ns\n", avg_alloc, avg_free);
    printf("Timed:     Avg alloc: %.2f ns, Avg free: %.2f ns\n", tavg_alloc, tavg_free);
    EXPECT_LT(avg_alloc, 10.0);   // Non-timed target <10ns
    EXPECT_LT(tavg_alloc, 25.0);  // Timed target <25ns
}

} // namespace
} // namespace hft 