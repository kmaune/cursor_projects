#include "hft/timing/hft_timer.hpp"
#include <arm_acle.h>
#include <arm_neon.h>
#include <algorithm>
#include <cmath>

namespace hft {

// Initialize static member with proper alignment
alignas(HFTTimer::CACHE_LINE_SIZE) std::atomic<double> HFTTimer::cycles_per_ns_{0.0};

// Initialize the timer system by calibrating cycles_per_ns_
void HFTTimer::initialize() noexcept {
    if (cycles_per_ns_.load(std::memory_order_relaxed) > 0.0) {
        return; // Already initialized
    }

    // Calibrate using high-resolution system clock
    const auto start_sys = std::chrono::steady_clock::now();
    const auto start_cycles = get_cycles();
    
    // Wait for a significant number of cycles to pass
    constexpr uint64_t CALIBRATION_CYCLES = 1000000;
    while (get_cycles() - start_cycles < CALIBRATION_CYCLES) {
        __builtin_arm_dmb(0xF); // Full memory barrier
    }
    
    const auto end_sys = std::chrono::steady_clock::now();
    const auto end_cycles = get_cycles();
    
    const auto sys_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_sys - start_sys).count();
    const auto cycles = end_cycles - start_cycles;
    
    cycles_per_ns_.store(static_cast<double>(cycles) / sys_ns,
                        std::memory_order_release);
}

HFTTimer::cycle_t HFTTimer::get_cycles() noexcept {
    uint64_t cycles;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(cycles));
    return cycles;
}

HFTTimer::ns_t HFTTimer::cycles_to_ns(cycle_t cycles) noexcept {
    initialize();
    return static_cast<ns_t>(cycles / cycles_per_ns_.load(std::memory_order_acquire));
}

HFTTimer::ns_t HFTTimer::get_timestamp_ns() noexcept {
    return cycles_to_ns(get_cycles());
}

// ScopedTimer implementation
HFTTimer::ScopedTimer::ScopedTimer(LatencyHistogram& histogram, std::string_view name) noexcept
    : histogram_(histogram)
    , name_(name)
    , start_cycles_(get_cycles()) {
}

HFTTimer::ScopedTimer::~ScopedTimer() noexcept {
    const auto end_cycles = get_cycles();
    const auto latency = cycles_to_ns(end_cycles - start_cycles_);
    histogram_.record_latency(latency);
}

// LatencyHistogram implementation
LatencyHistogram::LatencyHistogram(size_t num_bins) noexcept {
    reset();
}

void LatencyHistogram::record_latency(HFTTimer::ns_t latency) noexcept {
    // Update min/max atomically with proper memory ordering
    HFTTimer::ns_t current_min = min_latency_.load(std::memory_order_acquire);
    while (latency < current_min) {
        if (min_latency_.compare_exchange_weak(current_min, latency,
                                             std::memory_order_acq_rel)) {
            break;
        }
    }

    HFTTimer::ns_t current_max = max_latency_.load(std::memory_order_acquire);
    while (latency > current_max) {
        if (max_latency_.compare_exchange_weak(current_max, latency,
                                             std::memory_order_acq_rel)) {
            break;
        }
    }

    // Update running statistics with proper memory ordering
    total_samples_.fetch_add(1, std::memory_order_acq_rel);
    sum_latency_.fetch_add(static_cast<double>(latency), std::memory_order_acq_rel);
    sum_squared_latency_.fetch_add(static_cast<double>(latency) * latency,
                                 std::memory_order_acq_rel);

    // Update histogram bin with proper binning logic
    const size_t bin_index = latency == 0 ? 0 : 
        std::min(static_cast<size_t>(std::log2(latency)) + 1,
                HFTTimer::MAX_HISTOGRAM_BINS - 1);
    bins_[bin_index].fetch_add(1, std::memory_order_acq_rel);
}

void LatencyHistogram::reset() noexcept {
    // Use acquire-release ordering for all reset operations
    for (auto& bin : bins_) {
        bin.store(0, std::memory_order_release);
    }
    
    // Reset all statistics atomically
    total_samples_.store(0, std::memory_order_release);
    min_latency_.store(std::numeric_limits<HFTTimer::ns_t>::max(),
                      std::memory_order_release);
    max_latency_.store(0, std::memory_order_release);
    sum_latency_.store(0.0, std::memory_order_release);
    sum_squared_latency_.store(0.0, std::memory_order_release);
    
    // Ensure all stores are visible
    std::atomic_thread_fence(std::memory_order_release);
}

LatencyStats LatencyHistogram::get_stats() const noexcept {
    LatencyStats stats;
    
    // Take a snapshot of all data with proper memory ordering
    const uint64_t total = total_samples_.load(std::memory_order_acquire);
    if (total == 0) {
        stats.min_latency = std::numeric_limits<HFTTimer::ns_t>::max();
        stats.max_latency = 0;
        stats.mean_latency = 0;
        stats.std_dev = 0;
        stats.total_samples = 0;
        stats.percentiles.fill(0);
        return stats;
    }

    // Load all statistics with acquire ordering
    stats.total_samples = total;
    stats.min_latency = min_latency_.load(std::memory_order_acquire);
    stats.max_latency = max_latency_.load(std::memory_order_acquire);
    
    const double sum = sum_latency_.load(std::memory_order_acquire);
    const double sum_squared = sum_squared_latency_.load(std::memory_order_acquire);
    
    stats.mean_latency = sum / total;
    stats.std_dev = std::sqrt((sum_squared / total) - (stats.mean_latency * stats.mean_latency));

    // Calculate percentiles using a snapshot of the histogram
    std::array<uint64_t, HFTTimer::MAX_HISTOGRAM_BINS> bin_snapshot;
    for (size_t i = 0; i < HFTTimer::MAX_HISTOGRAM_BINS; ++i) {
        bin_snapshot[i] = bins_[i].load(std::memory_order_acquire);
    }

    // Calculate cumulative distribution
    std::array<uint64_t, HFTTimer::MAX_HISTOGRAM_BINS> cumulative;
    cumulative[0] = bin_snapshot[0];
    for (size_t i = 1; i < HFTTimer::MAX_HISTOGRAM_BINS; ++i) {
        cumulative[i] = cumulative[i-1] + bin_snapshot[i];
    }

    // Calculate percentiles
    const std::array<double, 4> percentiles = {0.5, 0.9, 0.95, 0.99};
    for (size_t i = 0; i < percentiles.size(); ++i) {
        const uint64_t target = static_cast<uint64_t>(total * percentiles[i]);
        size_t bin = 0;
        while (bin < HFTTimer::MAX_HISTOGRAM_BINS && cumulative[bin] < target) {
            ++bin;
        }
        
        // Convert bin index to latency value
        if (bin == 0) {
            stats.percentiles[i] = 0;
        } else if (bin >= HFTTimer::MAX_HISTOGRAM_BINS) {
            stats.percentiles[i] = static_cast<double>(1ULL << (HFTTimer::MAX_HISTOGRAM_BINS - 1));
        } else {
            // Linear interpolation between bins
            const double bin_start = bin == 0 ? 0 : static_cast<double>(1ULL << (bin - 1));
            const double bin_end = static_cast<double>(1ULL << bin);
            const double bin_fraction = static_cast<double>(target - cumulative[bin-1]) / 
                                      static_cast<double>(bin_snapshot[bin]);
            stats.percentiles[i] = bin_start + (bin_end - bin_start) * bin_fraction;
        }
    }

    return stats;
}

} // namespace hft 