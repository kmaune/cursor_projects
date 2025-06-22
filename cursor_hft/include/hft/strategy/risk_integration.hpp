#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <utility>
#include <type_traits>
#include <cassert>

#include "hft/timing/hft_timer.hpp"
#include "hft/market_data/treasury_instruments.hpp"

namespace hft {
namespace strategy {

using namespace hft::market_data;

// Use common cache line size to avoid redefinition
namespace {
    constexpr size_t CACHE_LINE_SIZE = 64;
}

// Multi-layer risk framework timing budgets
constexpr uint64_t LAYER1_FAST_LIMIT_BUDGET_NS = 50;    // Ultra-fast hard limits
constexpr uint64_t LAYER2_ENHANCED_BUDGET_NS = 400;     // Enhanced risk checks
constexpr uint64_t EMERGENCY_RESPONSE_BUDGET_NS = 100;  // Emergency actions

// Layer 1: Hard limits (50ns budget)
constexpr int64_t HARD_POSITION_LIMIT = 100000000;      // $100M per instrument
constexpr int64_t HARD_DAILY_LOSS_LIMIT = 1000000;      // $1M daily loss
constexpr uint32_t HARD_ORDER_FREQUENCY_LIMIT = 1000;   // 1000 orders/sec
constexpr double HARD_NOTIONAL_LIMIT = 500000000.0;     // $500M total notional
constexpr uint32_t HARD_MESSAGE_RATE_LIMIT = 10000;     // 10k messages/sec

// Layer 2: Enhanced risk checks (400ns budget)
constexpr double ENHANCED_DV01_LIMIT = 50000.0;         // $50k portfolio DV01
constexpr double ENHANCED_CONCENTRATION_LIMIT = 0.6;     // 60% max in one instrument
constexpr double ENHANCED_CORRELATION_LIMIT = 0.8;       // 80% max correlation exposure
constexpr double ENHANCED_VAR_LIMIT = 2000000.0;        // $2M Value-at-Risk limit
constexpr double ENHANCED_STRESS_LOSS_LIMIT = 5000000.0; // $5M stress loss limit

// Risk violation severity levels
enum class RiskViolationSeverity : uint8_t {
    NONE = 0,
    WARNING = 1,          // Soft limit approached
    VIOLATION = 2,        // Hard limit violated
    CRITICAL = 3,         // Emergency action required
    EMERGENCY = 4         // Immediate halt required
};

// Risk check results for different layers
enum class RiskCheckResult : uint8_t {
    APPROVED = 0,
    WARNING_ISSUED = 1,
    TRADE_REJECTED = 2,
    POSITION_REDUCE = 3,
    EMERGENCY_HALT = 4
};

// Layer 1: Ultra-fast hard limit state (cache-aligned)
struct alignas(CACHE_LINE_SIZE) Layer1RiskState {
    // Position limits per instrument
    std::array<std::atomic<int64_t>, 6> current_positions;    // 48 bytes
    
    // Daily counters
    std::atomic<int64_t> daily_realized_pnl;                 // 8 bytes
    std::atomic<uint32_t> orders_today;                      // 4 bytes
    std::atomic<uint32_t> messages_today;                    // 4 bytes
    
    Layer1RiskState() noexcept {
        for (auto& pos : current_positions) {
            pos.store(0, std::memory_order_relaxed);
        }
        daily_realized_pnl.store(0, std::memory_order_relaxed);
        orders_today.store(0, std::memory_order_relaxed);
        messages_today.store(0, std::memory_order_relaxed);
    }
};

// Layer 2: Enhanced risk metrics (cache-aligned)
struct alignas(CACHE_LINE_SIZE) Layer2RiskState {
    // Portfolio risk metrics
    std::atomic<double> portfolio_dv01;                      // 8 bytes
    std::atomic<double> portfolio_duration;                  // 8 bytes
    std::atomic<double> concentration_ratio;                 // 8 bytes
    std::atomic<double> correlation_exposure;                // 8 bytes
    std::atomic<double> value_at_risk;                       // 8 bytes
    std::atomic<double> stress_test_loss;                    // 8 bytes
    
    // Advanced metrics
    std::atomic<double> beta_to_benchmark;                   // 8 bytes
    std::atomic<double> tracking_error;                      // 8 bytes
    
    Layer2RiskState() noexcept {
        portfolio_dv01.store(0.0, std::memory_order_relaxed);
        portfolio_duration.store(0.0, std::memory_order_relaxed);
        concentration_ratio.store(0.0, std::memory_order_relaxed);
        correlation_exposure.store(0.0, std::memory_order_relaxed);
        value_at_risk.store(0.0, std::memory_order_relaxed);
        stress_test_loss.store(0.0, std::memory_order_relaxed);
        beta_to_benchmark.store(0.0, std::memory_order_relaxed);
        tracking_error.store(0.0, std::memory_order_relaxed);
    }
};

// Risk violation event for logging/monitoring
struct RiskViolationEvent {
    RiskViolationSeverity severity;
    TreasuryType instrument;
    uint32_t violation_type;              // Bitmask of violated limits
    double violation_magnitude;           // How much limit was exceeded (ratio)
    hft::HFTTimer::timestamp_t timestamp;
    uint64_t event_id;
    
    RiskViolationEvent() noexcept 
        : severity(RiskViolationSeverity::NONE), instrument(TreasuryType::Note_10Y),
          violation_type(0), violation_magnitude(0.0), timestamp(0), event_id(0) {}
};

// Comprehensive risk check request
struct RiskCheckRequest {
    TreasuryType instrument;
    int64_t proposed_quantity;            // Signed quantity change
    Price32nd proposed_price;
    bool use_enhanced_checks;             // Whether to run Layer 2 checks
    hft::HFTTimer::timestamp_t request_time;
    
    RiskCheckRequest() noexcept 
        : instrument(TreasuryType::Note_10Y), proposed_quantity(0),
          proposed_price(Price32nd::from_decimal(0.0)), use_enhanced_checks(false), request_time(0) {}
    
    RiskCheckRequest(TreasuryType instr, int64_t qty, Price32nd price, bool enhanced = false) noexcept
        : instrument(instr), proposed_quantity(qty), proposed_price(price),
          use_enhanced_checks(enhanced), request_time(hft::HFTTimer::get_timestamp_ns()) {}
};

// Risk check response with detailed information
struct RiskCheckResponse {
    RiskCheckResult result;
    RiskViolationSeverity max_severity;
    uint32_t violations_bitmask;          // Which limits were violated
    double recommended_size_reduction;    // If position reduction needed (0.0-1.0)
    uint64_t check_latency_ns;
    RiskViolationEvent violation_event;   // If violation occurred
    
    RiskCheckResponse() noexcept 
        : result(RiskCheckResult::APPROVED), max_severity(RiskViolationSeverity::NONE),
          violations_bitmask(0), recommended_size_reduction(0.0), check_latency_ns(0) {}
};

// Main multi-layer risk integration framework
class RiskIntegrationFramework {
private:
    // Multi-layer risk state (cache-aligned)
    alignas(CACHE_LINE_SIZE) Layer1RiskState layer1_state_;
    alignas(CACHE_LINE_SIZE) Layer2RiskState layer2_state_;
    
    // Emergency state tracking
    std::atomic<bool> emergency_halt_active_;
    std::atomic<hft::HFTTimer::timestamp_t> last_emergency_time_;
    
    // Performance tracking
    hft::HFTTimer timer_;
    mutable std::atomic<uint64_t> layer1_checks_processed_;
    mutable std::atomic<uint64_t> layer2_checks_processed_;
    mutable std::atomic<uint64_t> total_layer1_latency_ns_;
    mutable std::atomic<uint64_t> total_layer2_latency_ns_;
    mutable std::atomic<uint64_t> violations_detected_;
    
    // Risk event ID generation
    std::atomic<uint64_t> next_event_id_;
    
public:
    RiskIntegrationFramework() noexcept
        : emergency_halt_active_(false)
        , last_emergency_time_(0)
        , timer_()
        , layer1_checks_processed_(0)
        , layer2_checks_processed_(0)
        , total_layer1_latency_ns_(0)
        , total_layer2_latency_ns_(0)
        , violations_detected_(0)
        , next_event_id_(1) {}
    
    // Delete copy/move constructors
    RiskIntegrationFramework(const RiskIntegrationFramework&) = delete;
    RiskIntegrationFramework& operator=(const RiskIntegrationFramework&) = delete;
    RiskIntegrationFramework(RiskIntegrationFramework&&) = delete;
    RiskIntegrationFramework& operator=(RiskIntegrationFramework&&) = delete;
    
    // Layer 1: Ultra-fast hard limits check (target: 50ns)
    RiskCheckResult perform_layer1_check(const RiskCheckRequest& request) noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        // Check emergency halt status (5ns)
        if (emergency_halt_active_.load(std::memory_order_relaxed)) {
            return RiskCheckResult::EMERGENCY_HALT;
        }
        
        const auto instrument_index = static_cast<size_t>(request.instrument);
        
        // Check position limits (15ns)
        const auto current_position = layer1_state_.current_positions[instrument_index].load(std::memory_order_relaxed);
        const auto new_position = current_position + request.proposed_quantity;
        
        if (std::abs(new_position) > HARD_POSITION_LIMIT) {
            return RiskCheckResult::TRADE_REJECTED;
        }
        
        // Check daily loss limit (10ns)
        const auto daily_pnl = layer1_state_.daily_realized_pnl.load(std::memory_order_relaxed);
        if (daily_pnl < -HARD_DAILY_LOSS_LIMIT) {
            return RiskCheckResult::EMERGENCY_HALT;
        }
        
        // Check order frequency (10ns)
        const auto orders_today = layer1_state_.orders_today.load(std::memory_order_relaxed);
        if (orders_today >= HARD_ORDER_FREQUENCY_LIMIT) {
            return RiskCheckResult::TRADE_REJECTED;
        }
        
        // Check message rate (10ns)
        const auto messages_today = layer1_state_.messages_today.load(std::memory_order_relaxed);
        if (messages_today >= HARD_MESSAGE_RATE_LIMIT) {
            return RiskCheckResult::TRADE_REJECTED;
        }
        
        // Update performance tracking
        const auto end_time = timer_.get_timestamp_ns();
        layer1_checks_processed_.fetch_add(1, std::memory_order_relaxed);
        total_layer1_latency_ns_.fetch_add(end_time - start_time, std::memory_order_relaxed);
        
        return RiskCheckResult::APPROVED;
    }
    
    // Layer 2: Enhanced risk checks (target: 400ns)
    RiskCheckResult perform_layer2_check(const RiskCheckRequest& request) noexcept {
        const auto start_time = timer_.get_timestamp_ns();
        
        const auto instrument_index = static_cast<size_t>(request.instrument);
        auto result = RiskCheckResult::APPROVED;
        
        // DV01 portfolio limit check (80ns)
        const auto portfolio_dv01 = layer2_state_.portfolio_dv01.load(std::memory_order_relaxed);
        
        // Estimate DV01 change from proposed trade
        const double notional_millions = std::abs(request.proposed_quantity) / 1000000.0;
        const std::array<double, 6> dv01_per_million = {98.0, 196.0, 196.0, 472.0, 867.0, 1834.0};
        const double dv01_change = notional_millions * dv01_per_million[instrument_index];
        
        if (portfolio_dv01 + dv01_change > ENHANCED_DV01_LIMIT) {
            result = RiskCheckResult::POSITION_REDUCE;
        }
        
        // Concentration risk check (80ns)
        const auto concentration = layer2_state_.concentration_ratio.load(std::memory_order_relaxed);
        if (concentration > ENHANCED_CONCENTRATION_LIMIT) {
            if (result == RiskCheckResult::APPROVED) {
                result = RiskCheckResult::WARNING_ISSUED;
            }
        }
        
        // Correlation exposure check (80ns)
        const auto correlation_exposure = layer2_state_.correlation_exposure.load(std::memory_order_relaxed);
        if (correlation_exposure > ENHANCED_CORRELATION_LIMIT) {
            if (result == RiskCheckResult::APPROVED) {
                result = RiskCheckResult::WARNING_ISSUED;
            }
        }
        
        // Value-at-Risk check (80ns)
        const auto var = layer2_state_.value_at_risk.load(std::memory_order_relaxed);
        if (var > ENHANCED_VAR_LIMIT) {
            result = RiskCheckResult::POSITION_REDUCE;
        }
        
        // Stress test loss check (80ns)
        const auto stress_loss = layer2_state_.stress_test_loss.load(std::memory_order_relaxed);
        if (stress_loss > ENHANCED_STRESS_LOSS_LIMIT) {
            result = RiskCheckResult::EMERGENCY_HALT;
        }
        
        // Update performance tracking
        const auto end_time = timer_.get_timestamp_ns();
        layer2_checks_processed_.fetch_add(1, std::memory_order_relaxed);
        total_layer2_latency_ns_.fetch_add(end_time - start_time, std::memory_order_relaxed);
        
        return result;
    }
    
    // Comprehensive risk check with adaptive complexity
    RiskCheckResponse perform_comprehensive_check(const RiskCheckRequest& request) noexcept {
        RiskCheckResponse response;
        const auto overall_start = timer_.get_timestamp_ns();
        
        // Always perform Layer 1 check (50ns budget)
        response.result = perform_layer1_check(request);
        
        // If Layer 1 fails, return immediately
        if (response.result == RiskCheckResult::EMERGENCY_HALT ||
            response.result == RiskCheckResult::TRADE_REJECTED) {
            response.max_severity = RiskViolationSeverity::CRITICAL;
            response.check_latency_ns = timer_.get_timestamp_ns() - overall_start;
            violations_detected_.fetch_add(1, std::memory_order_relaxed);
            return response;
        }
        
        // Perform Layer 2 check if requested and budget allows
        if (request.use_enhanced_checks) {
            const auto layer2_result = perform_layer2_check(request);
            
            // Combine results - take more restrictive
            if (layer2_result > response.result) {
                response.result = layer2_result;
            }
            
            // Set severity based on combined results
            switch (response.result) {
                case RiskCheckResult::APPROVED:
                    response.max_severity = RiskViolationSeverity::NONE;
                    break;
                case RiskCheckResult::WARNING_ISSUED:
                    response.max_severity = RiskViolationSeverity::WARNING;
                    break;
                case RiskCheckResult::TRADE_REJECTED:
                    response.max_severity = RiskViolationSeverity::VIOLATION;
                    break;
                case RiskCheckResult::POSITION_REDUCE:
                    response.max_severity = RiskViolationSeverity::CRITICAL;
                    response.recommended_size_reduction = 0.5; // 50% reduction
                    break;
                case RiskCheckResult::EMERGENCY_HALT:
                    response.max_severity = RiskViolationSeverity::EMERGENCY;
                    activate_emergency_halt();
                    break;
            }
        }
        
        response.check_latency_ns = timer_.get_timestamp_ns() - overall_start;
        
        // Log violation if detected
        if (response.max_severity > RiskViolationSeverity::NONE) {
            response.violation_event = create_violation_event(request, response);
            violations_detected_.fetch_add(1, std::memory_order_relaxed);
        }
        
        return response;
    }
    
    // Update Layer 1 state after trade execution
    void update_layer1_state(TreasuryType instrument, int64_t quantity_change, 
                           int64_t realized_pnl_change = 0) noexcept {
        const auto instrument_index = static_cast<size_t>(instrument);
        
        // Update position
        layer1_state_.current_positions[instrument_index].fetch_add(quantity_change, std::memory_order_relaxed);
        
        // Update realized P&L
        if (realized_pnl_change != 0) {
            layer1_state_.daily_realized_pnl.fetch_add(realized_pnl_change, std::memory_order_relaxed);
        }
        
        // Increment order count
        layer1_state_.orders_today.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Update Layer 2 state (called periodically or after significant changes)
    void update_layer2_state(double new_portfolio_dv01, double new_concentration_ratio,
                           double new_correlation_exposure, double new_var, 
                           double new_stress_loss) noexcept {
        layer2_state_.portfolio_dv01.store(new_portfolio_dv01, std::memory_order_relaxed);
        layer2_state_.concentration_ratio.store(new_concentration_ratio, std::memory_order_relaxed);
        layer2_state_.correlation_exposure.store(new_correlation_exposure, std::memory_order_relaxed);
        layer2_state_.value_at_risk.store(new_var, std::memory_order_relaxed);
        layer2_state_.stress_test_loss.store(new_stress_loss, std::memory_order_relaxed);
    }
    
    // Emergency controls
    void activate_emergency_halt() noexcept {
        emergency_halt_active_.store(true, std::memory_order_relaxed);
        last_emergency_time_.store(timer_.get_timestamp_ns(), std::memory_order_relaxed);
    }
    
    void deactivate_emergency_halt() noexcept {
        emergency_halt_active_.store(false, std::memory_order_relaxed);
    }
    
    bool is_emergency_halt_active() const noexcept {
        return emergency_halt_active_.load(std::memory_order_relaxed);
    }
    
    // Risk state queries
    const Layer1RiskState& get_layer1_state() const noexcept {
        return layer1_state_; // Return reference to avoid copying atomic state
    }
    
    const Layer2RiskState& get_layer2_state() const noexcept {
        return layer2_state_; // Return reference to avoid copying atomic state
    }
    
    // Performance statistics
    struct RiskFrameworkStats {
        uint64_t layer1_checks_processed;
        uint64_t layer2_checks_processed;
        double average_layer1_latency_ns;
        double average_layer2_latency_ns;
        uint64_t total_violations_detected;
        double violation_rate;
        bool emergency_halt_active;
        hft::HFTTimer::timestamp_t last_emergency_time;
    };
    
    RiskFrameworkStats get_performance_stats() const noexcept {
        const auto layer1_checks = layer1_checks_processed_.load(std::memory_order_relaxed);
        const auto layer2_checks = layer2_checks_processed_.load(std::memory_order_relaxed);
        const auto layer1_latency = total_layer1_latency_ns_.load(std::memory_order_relaxed);
        const auto layer2_latency = total_layer2_latency_ns_.load(std::memory_order_relaxed);
        const auto violations = violations_detected_.load(std::memory_order_relaxed);
        
        RiskFrameworkStats stats{};
        stats.layer1_checks_processed = layer1_checks;
        stats.layer2_checks_processed = layer2_checks;
        stats.average_layer1_latency_ns = layer1_checks > 0 ? 
            static_cast<double>(layer1_latency) / layer1_checks : 0.0;
        stats.average_layer2_latency_ns = layer2_checks > 0 ? 
            static_cast<double>(layer2_latency) / layer2_checks : 0.0;
        stats.total_violations_detected = violations;
        stats.violation_rate = (layer1_checks + layer2_checks) > 0 ? 
            static_cast<double>(violations) / (layer1_checks + layer2_checks) : 0.0;
        stats.emergency_halt_active = emergency_halt_active_.load(std::memory_order_relaxed);
        stats.last_emergency_time = last_emergency_time_.load(std::memory_order_relaxed);
        
        return stats;
    }
    
private:
    // Create violation event for logging/monitoring
    RiskViolationEvent create_violation_event(const RiskCheckRequest& request,
                                             const RiskCheckResponse& response) noexcept {
        RiskViolationEvent event{};
        event.severity = response.max_severity;
        event.instrument = request.instrument;
        event.violation_type = response.violations_bitmask;
        event.violation_magnitude = 1.0; // Simplified - would calculate actual ratio
        event.timestamp = timer_.get_timestamp_ns();
        event.event_id = next_event_id_.fetch_add(1, std::memory_order_relaxed);
        
        return event;
    }
};

} // namespace strategy
} // namespace hft