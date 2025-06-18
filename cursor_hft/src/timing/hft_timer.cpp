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
    // Update min/max atomically
    HFTTimer::ns_t current_min = min_latency_.load(std::memory_order_relaxed);
    while (latency < current_min) {
        if (min_latency_.compare_exchange_weak(current_min, latency,
                                             std::memory_order_relaxed)) {
            break;
        }
    }

    HFTTimer::ns_t current_max = max_latency_.load(std::memory_order_relaxed);
    while (latency > current_max) {
        if (max_latency_.compare_exchange_weak(current_max, latency,
                                             std::memory_order_relaxed)) {
            break;
        }
    }

    // Update running statistics
    total_samples_.fetch_add(1, std::memory_order_relaxed);
    sum_latency_.fetch_add(static_cast<double>(latency), std::memory_order_relaxed);
    sum_squared_latency_.fetch_add(static_cast<double>(latency) * latency,
                                 std::memory_order_relaxed);

    // Update histogram bin
    const size_t bin_index = std::min(
        static_cast<size_t>(std::log2(latency + 1)),
        HFTTimer::MAX_HISTOGRAM_BINS - 1);
    bins_[bin_index].fetch_add(1, std::memory_order_relaxed);
}

void LatencyHistogram::reset() noexcept {
    for (auto& bin : bins_) {
        bin.store(0, std::memory_order_relaxed);
    }
    total_samples_.store(0, std::memory_order_relaxed);
    min_latency_.store(std::numeric_limits<HFTTimer::ns_t>::max(),
                      std::memory_order_relaxed);
    max_latency_.store(0, std::memory_order_relaxed);
    sum_latency_.store(0.0, std::memory_order_relaxed);
    sum_squared_latency_.store(0.0, std::memory_order_relaxed);
}

LatencyStats LatencyHistogram::get_stats() const noexcept {
    LatencyStats stats;
    const uint64_t total = total_samples_.load(std::memory_order_acquire);
    
    if (total == 0) {
        return stats;
    }

    stats.total_samples = total;
    stats.min_latency = min_latency_.load(std::memory_order_acquire);
    stats.max_latency = max_latency_.load(std::memory_order_acquire);
    
    const double sum = sum_latency_.load(std::memory_order_acquire);
    const double sum_squared = sum_squared_latency_.load(std::memory_order_acquire);
    
    stats.mean_latency = sum / total;
    stats.std_dev = std::sqrt((sum_squared / total) - (stats.mean_latency * stats.mean_latency));

    // Calculate percentiles
    const std::array<double, 4> percentiles = {0.5, 0.9, 0.95, 0.99};
    std::array<uint64_t, 4> targets;
    for (size_t i = 0; i < percentiles.size(); ++i) {
        targets[i] = static_cast<uint64_t>(total * percentiles[i]);
    }

    uint64_t cumulative = 0;
    for (size_t bin = 0; bin < HFTTimer::MAX_HISTOGRAM_BINS; ++bin) {
        cumulative += bins_[bin].load(std::memory_order_acquire);
        for (size_t i = 0; i < percentiles.size(); ++i) {
            if (cumulative >= targets[i] && stats.percentiles[i] == 0) {
                stats.percentiles[i] = static_cast<double>(1ULL << bin);
            }
        }
    }

    return stats;
}

} // namespace hft 