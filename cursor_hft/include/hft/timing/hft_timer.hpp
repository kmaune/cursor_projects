#pragma once

#include <cstdint>
#include <atomic>
#include <array>
#include <chrono>
#include <string_view>
#include <memory>
#include <vector>

namespace hft {

// Forward declarations
class LatencyHistogram;
class LatencyStats;

/**
 * @brief High-performance timer using ARM64 cycle counter
 * 
 * This class provides nanosecond-precision timing using the ARM64 cycle counter
 * (cntvct_el0) with zero allocation in hot paths. All timing operations are
 * lock-free and thread-safe.
 */
class HFTTimer {
public:
    // Type aliases for clarity
    using cycle_t = uint64_t;
    using ns_t = uint64_t;
    using timestamp_t = uint64_t;

    // Constants
    static constexpr size_t MAX_HISTOGRAM_BINS = 1024;
    static constexpr size_t CACHE_LINE_SIZE = 64;

    /**
     * @brief Get current timestamp in cycles
     * @return Current cycle count from cntvct_el0
     */
    [[nodiscard]] static cycle_t get_cycles() noexcept;

    /**
     * @brief Convert cycles to nanoseconds
     * @param cycles Number of cycles to convert
     * @return Equivalent time in nanoseconds
     */
    [[nodiscard]] static ns_t cycles_to_ns(cycle_t cycles) noexcept;

    /**
     * @brief Get current timestamp in nanoseconds
     * @return Current time in nanoseconds
     */
    [[nodiscard]] static ns_t get_timestamp_ns() noexcept;

    /**
     * @brief RAII wrapper for timing a scope
     */
    class ScopedTimer {
    public:
        explicit ScopedTimer(LatencyHistogram& histogram, std::string_view name = "") noexcept;
        ~ScopedTimer() noexcept;

        // Prevent copying
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

    private:
        LatencyHistogram& histogram_;
        std::string_view name_;
        cycle_t start_cycles_;
    };

private:
    // Cache-aligned frequency storage
    alignas(CACHE_LINE_SIZE) static std::atomic<double> cycles_per_ns_;
    
    // Initialize the timer system
    static void initialize() noexcept;
};

/**
 * @brief Thread-safe latency histogram for collecting timing statistics
 */
class LatencyHistogram {
public:
    explicit LatencyHistogram(size_t num_bins = HFTTimer::MAX_HISTOGRAM_BINS) noexcept;
    
    // Record a latency measurement
    void record_latency(HFTTimer::ns_t latency) noexcept;
    
    // Get statistical analysis of collected measurements
    [[nodiscard]] LatencyStats get_stats() const noexcept;
    
    // Reset all collected data
    void reset() noexcept;

private:
    // Cache-aligned bin storage
    alignas(HFTTimer::CACHE_LINE_SIZE) std::array<std::atomic<uint64_t>, HFTTimer::MAX_HISTOGRAM_BINS> bins_;
    alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<uint64_t> total_samples_;
    alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<HFTTimer::ns_t> min_latency_;
    alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<HFTTimer::ns_t> max_latency_;
    alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<double> sum_latency_;
    alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<double> sum_squared_latency_;
};

/**
 * @brief Statistical analysis of latency measurements
 */
class LatencyStats {
public:
    HFTTimer::ns_t min_latency;
    HFTTimer::ns_t max_latency;
    double mean_latency;
    double std_dev;
    std::array<double, 4> percentiles; // 50th, 90th, 95th, 99th
    uint64_t total_samples;
};

} // namespace hft 