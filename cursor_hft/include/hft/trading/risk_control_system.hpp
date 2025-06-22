#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include <cmath>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/trading/order_lifecycle_manager.hpp"

namespace hft {
namespace trading {

using namespace hft::market_data;

/**
 * @brief Real-time risk control system with circuit breakers
 * 
 * Implements comprehensive risk management:
 * - Real-time position and P&L monitoring
 * - Multi-level circuit breakers
 * - Order rate limiting and validation
 * - Market volatility circuit breakers
 * - Concentration risk limits
 * - Emergency stop mechanisms
 * - Risk breach alerting and logging
 * 
 * Performance targets:
 * - Risk check: <100ns
 * - Circuit breaker evaluation: <50ns
 * - P&L calculation: <75ns
 * - Position update: <25ns
 * - Breach detection: <30ns
 */
class alignas(64) RiskControlSystem {
public:
    static constexpr size_t MAX_INSTRUMENTS = 6;
    static constexpr size_t MAX_STRATEGIES = 8;
    static constexpr size_t RISK_HISTORY_SIZE = 10000;
    static constexpr size_t VOLATILITY_WINDOW = 1000;
    
    // Risk breach severity levels
    enum class RiskSeverity : uint8_t {
        INFO = 0,           // Informational
        WARNING = 1,        // Warning level
        CRITICAL = 2,       // Critical level
        EMERGENCY = 3       // Emergency stop required
    };
    
    // Circuit breaker types
    enum class CircuitBreakerType : uint8_t {
        POSITION_LIMIT = 0,     // Position size limit
        PNL_LOSS_LIMIT = 1,     // P&L loss limit
        ORDER_RATE_LIMIT = 2,   // Order rate limit
        VOLATILITY_LIMIT = 3,   // Market volatility limit
        CONCENTRATION_LIMIT = 4, // Concentration risk limit
        DRAWDOWN_LIMIT = 5,     // Maximum drawdown limit
        VAR_LIMIT = 6,          // Value at Risk limit
        LEVERAGE_LIMIT = 7      // Leverage limit
    };
    
    // Real-time risk metrics per instrument
    struct alignas(64) InstrumentRisk {
        TreasuryType instrument;                            // Instrument type (1 byte)
        uint8_t _pad0[7];                                   // Padding (7 bytes)
        int64_t net_position = 0;                           // Net position (8 bytes)
        double unrealized_pnl = 0.0;                        // Unrealized P&L (8 bytes)
        double realized_pnl = 0.0;                          // Realized P&L (8 bytes)
        double daily_pnl = 0.0;                             // Daily P&L (8 bytes)
        double value_at_risk = 0.0;                         // 1-day VaR (8 bytes)
        double market_value = 0.0;                          // Current market value (8 bytes)
        uint64_t last_update_time_ns = 0;                   // Last update timestamp (8 bytes)
        // Total: 1+7+8+8+8+8+8+8+8 = 64 bytes
        
        InstrumentRisk() noexcept : _pad0{} {}
    };
    static_assert(sizeof(InstrumentRisk) == 64, "InstrumentRisk must be 64 bytes");
    
    // Risk limits configuration
    struct RiskLimits {
        // Position limits
        int64_t max_position_per_instrument = 100000000;    // Maximum position per instrument
        int64_t max_total_position = 500000000;             // Maximum total position
        double max_concentration_ratio = 0.3;               // Maximum concentration per instrument
        
        // P&L limits
        double max_daily_loss = 1000000.0;                  // Maximum daily loss
        double max_unrealized_loss = 2000000.0;             // Maximum unrealized loss
        double max_drawdown = 5000000.0;                    // Maximum drawdown from peak
        
        // Risk metrics limits
        double max_value_at_risk = 500000.0;                // Maximum 1-day VaR
        double max_leverage_ratio = 10.0;                   // Maximum leverage
        double max_portfolio_correlation = 0.8;             // Maximum correlation
        
        // Rate limits
        uint32_t max_orders_per_second = 1000;              // Order rate limit
        uint32_t max_cancels_per_minute = 5000;             // Cancel rate limit
        uint32_t max_order_size = 50000000;                 // Maximum single order size
        
        // Volatility limits
        double max_price_volatility = 0.05;                 // 5% maximum price volatility
        double volatility_window_minutes = 5.0;             // Volatility calculation window
        
        RiskLimits() noexcept = default;
    };
    
    // Circuit breaker state
    struct alignas(64) CircuitBreaker {
        CircuitBreakerType type;                            // Breaker type (1 byte)
        bool active = false;                                // Currently active (1 byte)
        RiskSeverity severity = RiskSeverity::WARNING;      // Severity level (1 byte)
        uint8_t _pad0[5];                                   // Padding (5 bytes)
        double threshold = 0.0;                             // Threshold value (8 bytes)
        double current_value = 0.0;                         // Current measured value (8 bytes)
        uint64_t trigger_time_ns = 0;                       // Time when triggered (8 bytes)
        uint64_t reset_time_ns = 0;                         // Time when reset (8 bytes)
        uint32_t trigger_count = 0;                         // Number of triggers today (4 bytes)
        uint32_t _reserved = 0;                             // Reserved (4 bytes)
        char description[16];                               // Description (16 bytes)
        // Total: 1+1+1+5+8+8+8+8+4+4+16 = 64 bytes
        
        CircuitBreaker() noexcept : _pad0{} { description[0] = '\0'; }
    };
    static_assert(sizeof(CircuitBreaker) == 64, "CircuitBreaker must be 64 bytes");
    
    // Risk breach event for logging
    struct alignas(64) RiskBreach {
        uint64_t event_id = 0;                              // Unique event ID (8 bytes)
        uint64_t timestamp_ns = 0;                          // Event timestamp (8 bytes)
        CircuitBreakerType breaker_type;                    // Breaker type (1 byte)
        RiskSeverity severity;                              // Severity level (1 byte)
        TreasuryType instrument;                            // Related instrument (1 byte)
        uint8_t _pad0[5];                                   // Padding (5 bytes)
        double threshold_value = 0.0;                       // Threshold that was breached (8 bytes)
        double actual_value = 0.0;                          // Actual value at breach (8 bytes)
        double percentage_over = 0.0;                       // Percentage over threshold (8 bytes)
        char action_taken[16];                              // Action taken (16 bytes)
        // Total: 8+8+1+1+1+5+8+8+8+16 = 64 bytes
        
        RiskBreach() noexcept : _pad0{} { action_taken[0] = '\0'; }
    };
    static_assert(sizeof(RiskBreach) == 64, "RiskBreach must be 64 bytes");
    
    // Order rate tracking
    struct OrderRateTracker {
        std::array<uint64_t, 60> orders_per_second;         // Rolling 60-second window
        std::array<uint64_t, 60> cancels_per_second;        // Rolling 60-second window
        size_t current_second_index = 0;
        uint64_t last_update_time_ns = 0;
    };
    
    // Market volatility tracking
    struct VolatilityTracker {
        std::array<double, VOLATILITY_WINDOW> price_history;
        size_t current_index = 0;
        uint64_t sample_count = 0;
        double current_volatility = 0.0;
        uint64_t last_calculation_time_ns = 0;
    };
    
    /**
     * @brief Constructor with risk configuration
     */
    RiskControlSystem(const RiskLimits& limits = RiskLimits{}) noexcept;
    
    // No copy or move semantics
    RiskControlSystem(const RiskControlSystem&) = delete;
    RiskControlSystem& operator=(const RiskControlSystem&) = delete;
    
    /**
     * @brief Check if order is permitted by risk controls
     * @param instrument Treasury instrument
     * @param side Order side
     * @param quantity Order quantity
     * @param price Order price
     * @return true if order passes risk checks
     */
    [[nodiscard]] bool check_order_risk(
        TreasuryType instrument,
        OrderSide side,
        uint64_t quantity,
        Price32nd price
    ) noexcept;
    
    /**
     * @brief Update position and recalculate risk metrics
     * @param instrument Treasury instrument
     * @param position_change Position change (signed)
     * @param fill_price Fill price
     */
    void update_position(
        TreasuryType instrument,
        int64_t position_change,
        Price32nd fill_price
    ) noexcept;
    
    /**
     * @brief Update market prices for risk calculations
     * @param instrument Treasury instrument
     * @param market_price Current market price
     */
    void update_market_price(
        TreasuryType instrument,
        Price32nd market_price
    ) noexcept;
    
    /**
     * @brief Record order submission for rate limiting
     * @param is_cancel true if this is a cancellation
     */
    void record_order_activity(bool is_cancel = false) noexcept;
    
    /**
     * @brief Check all circuit breakers and update status
     * @return true if any circuit breaker is active
     */
    [[nodiscard]] bool evaluate_circuit_breakers() noexcept;
    
    /**
     * @brief Force trigger emergency stop
     * @param reason Reason for emergency stop
     */
    void trigger_emergency_stop(const char* reason) noexcept;
    
    /**
     * @brief Reset specific circuit breaker
     * @param type Circuit breaker type to reset
     * @return true if reset successful
     */
    bool reset_circuit_breaker(CircuitBreakerType type) noexcept;
    
    /**
     * @brief Get current risk metrics for instrument
     * @param instrument Treasury instrument
     * @return Risk metrics
     */
    [[nodiscard]] const InstrumentRisk& get_instrument_risk(TreasuryType instrument) const noexcept;
    
    /**
     * @brief Get circuit breaker status
     * @param type Circuit breaker type
     * @return Circuit breaker state
     */
    [[nodiscard]] const CircuitBreaker& get_circuit_breaker(CircuitBreakerType type) const noexcept;
    
    /**
     * @brief Get total portfolio P&L
     * @return Total unrealized + realized P&L
     */
    [[nodiscard]] double get_total_pnl() const noexcept;
    
    /**
     * @brief Get total portfolio Value at Risk
     * @return Portfolio VaR
     */
    [[nodiscard]] double get_portfolio_var() const noexcept;
    
    /**
     * @brief Check if any circuit breaker is active
     * @return true if any breaker is active
     */
    [[nodiscard]] bool is_emergency_stop_active() const noexcept { return emergency_stop_active_.load(); }
    
    /**
     * @brief Update risk limits configuration
     * @param limits New risk limits
     */
    void update_risk_limits(const RiskLimits& limits) noexcept;

private:
    // Risk configuration
    alignas(64) RiskLimits risk_limits_;
    alignas(64) hft::HFTTimer timer_;
    
    // Risk tracking per instrument
    alignas(64) std::array<InstrumentRisk, MAX_INSTRUMENTS> instrument_risks_;
    alignas(64) std::array<VolatilityTracker, MAX_INSTRUMENTS> volatility_trackers_;
    
    // Circuit breakers
    alignas(64) std::array<CircuitBreaker, 8> circuit_breakers_;  // One for each type
    alignas(64) std::atomic<bool> emergency_stop_active_;
    
    // Rate limiting
    alignas(64) OrderRateTracker rate_tracker_;
    
    // Risk breach logging
    alignas(64) std::array<RiskBreach, RISK_HISTORY_SIZE> risk_breach_history_;
    alignas(64) std::atomic<size_t> breach_history_index_;
    
    // Performance tracking
    alignas(64) std::atomic<uint64_t> risk_checks_performed_;
    alignas(64) std::atomic<uint64_t> total_risk_check_time_ns_;
    
    // Helper methods
    void initialize_circuit_breakers() noexcept;
    void calculate_portfolio_metrics() noexcept;
    void calculate_volatility(TreasuryType instrument) noexcept;
    void trigger_circuit_breaker(CircuitBreakerType type, double threshold, double actual, const char* reason) noexcept;
    void log_risk_breach(CircuitBreakerType type, RiskSeverity severity, TreasuryType instrument, 
                        double threshold, double actual, const char* action) noexcept;
    
    bool check_position_limits(TreasuryType instrument, OrderSide side, uint64_t quantity) noexcept;
    bool check_pnl_limits() noexcept;
    bool check_rate_limits() noexcept;
    bool check_volatility_limits(TreasuryType instrument) noexcept;
    bool check_concentration_limits() noexcept;
    
    double calculate_position_var(TreasuryType instrument, int64_t position) const noexcept;
    double calculate_portfolio_correlation() const noexcept;
};

// Implementation

inline RiskControlSystem::RiskControlSystem(const RiskLimits& limits) noexcept
    : risk_limits_(limits),
      timer_(),
      instrument_risks_{},
      volatility_trackers_{},
      circuit_breakers_{},
      emergency_stop_active_(false),
      rate_tracker_{},
      risk_breach_history_{},
      breach_history_index_(0),
      risk_checks_performed_(0),
      total_risk_check_time_ns_(0) {
    
    // Initialize instrument risks
    for (size_t i = 0; i < MAX_INSTRUMENTS; ++i) {
        instrument_risks_[i].instrument = static_cast<TreasuryType>(i);
    }
    
    // Initialize circuit breakers
    initialize_circuit_breakers();
}

inline void RiskControlSystem::initialize_circuit_breakers() noexcept {
    // Position limit breaker
    auto& pos_breaker = circuit_breakers_[static_cast<size_t>(CircuitBreakerType::POSITION_LIMIT)];
    pos_breaker.type = CircuitBreakerType::POSITION_LIMIT;
    pos_breaker.threshold = static_cast<double>(risk_limits_.max_total_position);
    pos_breaker.severity = RiskSeverity::CRITICAL;
    std::strcpy(pos_breaker.description, "Position Limit");
    
    // P&L loss limit breaker
    auto& pnl_breaker = circuit_breakers_[static_cast<size_t>(CircuitBreakerType::PNL_LOSS_LIMIT)];
    pnl_breaker.type = CircuitBreakerType::PNL_LOSS_LIMIT;
    pnl_breaker.threshold = -risk_limits_.max_daily_loss;
    pnl_breaker.severity = RiskSeverity::EMERGENCY;
    std::strcpy(pnl_breaker.description, "Daily Loss Limit");
    
    // Order rate limit breaker
    auto& rate_breaker = circuit_breakers_[static_cast<size_t>(CircuitBreakerType::ORDER_RATE_LIMIT)];
    rate_breaker.type = CircuitBreakerType::ORDER_RATE_LIMIT;
    rate_breaker.threshold = static_cast<double>(risk_limits_.max_orders_per_second);
    rate_breaker.severity = RiskSeverity::WARNING;
    std::strcpy(rate_breaker.description, "Order Rate Limit");
    
    // Volatility limit breaker
    auto& vol_breaker = circuit_breakers_[static_cast<size_t>(CircuitBreakerType::VOLATILITY_LIMIT)];
    vol_breaker.type = CircuitBreakerType::VOLATILITY_LIMIT;
    vol_breaker.threshold = risk_limits_.max_price_volatility;
    vol_breaker.severity = RiskSeverity::CRITICAL;
    std::strcpy(vol_breaker.description, "Volatility Limit");
    
    // VaR limit breaker
    auto& var_breaker = circuit_breakers_[static_cast<size_t>(CircuitBreakerType::VAR_LIMIT)];
    var_breaker.type = CircuitBreakerType::VAR_LIMIT;
    var_breaker.threshold = risk_limits_.max_value_at_risk;
    var_breaker.severity = RiskSeverity::CRITICAL;
    std::strcpy(var_breaker.description, "VaR Limit");
}

inline bool RiskControlSystem::check_order_risk(
    TreasuryType instrument,
    OrderSide side,
    uint64_t quantity,
    Price32nd price
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    // Check emergency stop
    if (emergency_stop_active_.load(std::memory_order_acquire)) {
        return false;
    }
    
    // Check all risk limits
    bool risk_passed = true;
    
    risk_passed &= check_position_limits(instrument, side, quantity);
    risk_passed &= check_pnl_limits();
    risk_passed &= check_rate_limits();
    risk_passed &= check_volatility_limits(instrument);
    risk_passed &= check_concentration_limits();
    
    // Update performance metrics
    risk_checks_performed_.fetch_add(1, std::memory_order_relaxed);
    const auto check_time = timer_.get_timestamp_ns() - start_time;
    total_risk_check_time_ns_.fetch_add(check_time, std::memory_order_relaxed);
    
    return risk_passed;
}

inline bool RiskControlSystem::check_position_limits(
    TreasuryType instrument,
    OrderSide side,
    uint64_t quantity
) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= MAX_INSTRUMENTS) return false;
    
    const auto& risk = instrument_risks_[instrument_index];
    
    // Calculate position change
    const int64_t position_change = (side == OrderSide::BUY) ? 
                                   static_cast<int64_t>(quantity) : 
                                   -static_cast<int64_t>(quantity);
    
    const int64_t new_position = risk.net_position + position_change;
    
    // Check per-instrument position limit
    if (std::abs(new_position) > risk_limits_.max_position_per_instrument) {
        trigger_circuit_breaker(CircuitBreakerType::POSITION_LIMIT, 
                               static_cast<double>(risk_limits_.max_position_per_instrument),
                               static_cast<double>(std::abs(new_position)),
                               "Instrument position limit exceeded");
        return false;
    }
    
    // Check total position limit
    int64_t total_position = 0;
    for (const auto& inst_risk : instrument_risks_) {
        if (inst_risk.instrument == instrument) {
            total_position += new_position;
        } else {
            total_position += inst_risk.net_position;
        }
    }
    
    if (std::abs(total_position) > risk_limits_.max_total_position) {
        trigger_circuit_breaker(CircuitBreakerType::POSITION_LIMIT,
                               static_cast<double>(risk_limits_.max_total_position),
                               static_cast<double>(std::abs(total_position)),
                               "Total position limit exceeded");
        return false;
    }
    
    return true;
}

inline bool RiskControlSystem::check_pnl_limits() noexcept {
    const double total_pnl = get_total_pnl();
    
    // Check daily loss limit
    if (total_pnl < -risk_limits_.max_daily_loss) {
        trigger_circuit_breaker(CircuitBreakerType::PNL_LOSS_LIMIT,
                               -risk_limits_.max_daily_loss,
                               total_pnl,
                               "Daily loss limit exceeded");
        return false;
    }
    
    return true;
}

inline bool RiskControlSystem::check_rate_limits() noexcept {
    // Simple rate check - count orders in last second
    const uint64_t current_time = timer_.get_timestamp_ns();
    const size_t current_second = (current_time / 1000000000) % 60;
    
    if (rate_tracker_.orders_per_second[current_second] >= risk_limits_.max_orders_per_second) {
        trigger_circuit_breaker(CircuitBreakerType::ORDER_RATE_LIMIT,
                               static_cast<double>(risk_limits_.max_orders_per_second),
                               static_cast<double>(rate_tracker_.orders_per_second[current_second]),
                               "Order rate limit exceeded");
        return false;
    }
    
    return true;
}

inline bool RiskControlSystem::check_volatility_limits(TreasuryType instrument) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= MAX_INSTRUMENTS) return true;
    
    const auto& tracker = volatility_trackers_[instrument_index];
    
    if (tracker.current_volatility > risk_limits_.max_price_volatility) {
        trigger_circuit_breaker(CircuitBreakerType::VOLATILITY_LIMIT,
                               risk_limits_.max_price_volatility,
                               tracker.current_volatility,
                               "Price volatility limit exceeded");
        return false;
    }
    
    return true;
}

inline void RiskControlSystem::update_position(
    TreasuryType instrument,
    int64_t position_change,
    Price32nd fill_price
) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= MAX_INSTRUMENTS) return;
    
    auto& risk = instrument_risks_[instrument_index];
    
    // Update position
    risk.net_position += position_change;
    
    // Update realized P&L (simple calculation)
    if (position_change != 0) {
        const double trade_value = static_cast<double>(std::abs(position_change)) * fill_price.to_decimal();
        // Simplified P&L calculation - in production would track weighted average cost
        risk.realized_pnl += (position_change > 0) ? -trade_value : trade_value;
    }
    
    risk.last_update_time_ns = timer_.get_timestamp_ns();
    
    // Recalculate portfolio metrics
    calculate_portfolio_metrics();
}

inline void RiskControlSystem::update_market_price(
    TreasuryType instrument,
    Price32nd market_price
) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= MAX_INSTRUMENTS) return;
    
    auto& risk = instrument_risks_[instrument_index];
    auto& volatility = volatility_trackers_[instrument_index];
    
    // Update market value
    risk.market_value = static_cast<double>(risk.net_position) * market_price.to_decimal();
    
    // Update volatility tracking
    const size_t vol_index = volatility.current_index % VOLATILITY_WINDOW;
    volatility.price_history[vol_index] = market_price.to_decimal();
    volatility.current_index++;
    volatility.sample_count = std::min(volatility.sample_count + 1, static_cast<uint64_t>(VOLATILITY_WINDOW));
    
    // Calculate volatility
    calculate_volatility(instrument);
    
    risk.last_update_time_ns = timer_.get_timestamp_ns();
}

inline void RiskControlSystem::calculate_volatility(TreasuryType instrument) noexcept {
    const auto instrument_index = static_cast<size_t>(instrument);
    if (instrument_index >= MAX_INSTRUMENTS) return;
    
    auto& tracker = volatility_trackers_[instrument_index];
    
    if (tracker.sample_count < 2) {
        tracker.current_volatility = 0.0;
        return;
    }
    
    // Calculate standard deviation of price returns
    double sum_squared_returns = 0.0;
    size_t count = 0;
    
    const size_t samples = std::min(tracker.sample_count, static_cast<uint64_t>(VOLATILITY_WINDOW));
    
    for (size_t i = 1; i < samples; ++i) {
        const size_t idx = (tracker.current_index - i) % VOLATILITY_WINDOW;
        const size_t prev_idx = (tracker.current_index - i - 1) % VOLATILITY_WINDOW;
        
        const double return_val = (tracker.price_history[idx] - tracker.price_history[prev_idx]) / 
                                 tracker.price_history[prev_idx];
        sum_squared_returns += return_val * return_val;
        ++count;
    }
    
    if (count > 0) {
        const double variance = sum_squared_returns / count;
        tracker.current_volatility = std::sqrt(variance);
    }
    
    tracker.last_calculation_time_ns = timer_.get_timestamp_ns();
}

inline void RiskControlSystem::record_order_activity(bool is_cancel) noexcept {
    const uint64_t current_time = timer_.get_timestamp_ns();
    const size_t current_second = (current_time / 1000000000) % 60;
    
    // Reset counter if we've moved to a new second
    if (current_time - rate_tracker_.last_update_time_ns > 1000000000) { // 1 second
        const size_t old_second = (rate_tracker_.last_update_time_ns / 1000000000) % 60;
        if (old_second != current_second) {
            rate_tracker_.orders_per_second[current_second] = 0;
            rate_tracker_.cancels_per_second[current_second] = 0;
        }
    }
    
    if (is_cancel) {
        rate_tracker_.cancels_per_second[current_second]++;
    } else {
        rate_tracker_.orders_per_second[current_second]++;
    }
    
    rate_tracker_.last_update_time_ns = current_time;
}

inline bool RiskControlSystem::evaluate_circuit_breakers() noexcept {
    bool any_active = false;
    
    for (auto& breaker : circuit_breakers_) {
        bool should_trigger = false;
        
        switch (breaker.type) {
            case CircuitBreakerType::POSITION_LIMIT: {
                int64_t total_position = 0;
                for (const auto& risk : instrument_risks_) {
                    total_position += std::abs(risk.net_position);
                }
                breaker.current_value = static_cast<double>(total_position);
                should_trigger = (total_position > risk_limits_.max_total_position);
                break;
            }
            
            case CircuitBreakerType::PNL_LOSS_LIMIT:
                breaker.current_value = get_total_pnl();
                should_trigger = (breaker.current_value < breaker.threshold);
                break;
                
            case CircuitBreakerType::VAR_LIMIT:
                breaker.current_value = get_portfolio_var();
                should_trigger = (breaker.current_value > breaker.threshold);
                break;
                
            default:
                break;
        }
        
        if (should_trigger && !breaker.active) {
            breaker.active = true;
            breaker.trigger_time_ns = timer_.get_timestamp_ns();
            breaker.trigger_count++;
            any_active = true;
            
            if (breaker.severity == RiskSeverity::EMERGENCY) {
                emergency_stop_active_.store(true, std::memory_order_release);
            }
        }
    }
    
    return any_active;
}

inline void RiskControlSystem::trigger_circuit_breaker(
    CircuitBreakerType type,
    double threshold,
    double actual,
    const char* reason
) noexcept {
    const auto type_index = static_cast<size_t>(type);
    if (type_index >= circuit_breakers_.size()) return;
    
    auto& breaker = circuit_breakers_[type_index];
    
    if (!breaker.active) {
        breaker.active = true;
        breaker.current_value = actual;
        breaker.trigger_time_ns = timer_.get_timestamp_ns();
        breaker.trigger_count++;
        
        // Log breach
        log_risk_breach(type, breaker.severity, TreasuryType::Note_10Y, threshold, actual, reason);
        
        // Check if emergency stop required
        if (breaker.severity == RiskSeverity::EMERGENCY) {
            trigger_emergency_stop(reason);
        }
    }
}

inline void RiskControlSystem::log_risk_breach(
    CircuitBreakerType type,
    RiskSeverity severity,
    TreasuryType instrument,
    double threshold,
    double actual,
    const char* action
) noexcept {
    const size_t index = breach_history_index_.fetch_add(1, std::memory_order_relaxed) % RISK_HISTORY_SIZE;
    
    auto& breach = risk_breach_history_[index];
    breach.event_id = index;
    breach.timestamp_ns = timer_.get_timestamp_ns();
    breach.breaker_type = type;
    breach.severity = severity;
    breach.instrument = instrument;
    breach.threshold_value = threshold;
    breach.actual_value = actual;
    breach.percentage_over = ((actual - threshold) / threshold) * 100.0;
    
    // Copy action string safely
    const size_t max_len = sizeof(breach.action_taken) - 1;
    size_t i = 0;
    while (i < max_len && action[i] != '\0') {
        breach.action_taken[i] = action[i];
        ++i;
    }
    breach.action_taken[i] = '\0';
}

inline double RiskControlSystem::get_total_pnl() const noexcept {
    double total = 0.0;
    for (const auto& risk : instrument_risks_) {
        total += risk.unrealized_pnl + risk.realized_pnl;
    }
    return total;
}

inline double RiskControlSystem::get_portfolio_var() const noexcept {
    // Simplified VaR calculation
    double total_var = 0.0;
    for (const auto& risk : instrument_risks_) {
        total_var += risk.value_at_risk;
    }
    return total_var;
}

// Getter implementations
inline const RiskControlSystem::InstrumentRisk& 
RiskControlSystem::get_instrument_risk(TreasuryType instrument) const noexcept {
    const auto index = static_cast<size_t>(instrument);
    return (index < MAX_INSTRUMENTS) ? instrument_risks_[index] : instrument_risks_[0];
}

inline const RiskControlSystem::CircuitBreaker& 
RiskControlSystem::get_circuit_breaker(CircuitBreakerType type) const noexcept {
    const auto index = static_cast<size_t>(type);
    return (index < circuit_breakers_.size()) ? circuit_breakers_[index] : circuit_breakers_[0];
}

// Stub implementations for remaining methods
inline void RiskControlSystem::trigger_emergency_stop(const char* reason) noexcept {
    emergency_stop_active_.store(true, std::memory_order_release);
}

inline bool RiskControlSystem::reset_circuit_breaker(CircuitBreakerType type) noexcept {
    const auto index = static_cast<size_t>(type);
    if (index >= circuit_breakers_.size()) return false;
    
    auto& breaker = circuit_breakers_[index];
    breaker.active = false;
    breaker.reset_time_ns = timer_.get_timestamp_ns();
    
    return true;
}

inline void RiskControlSystem::update_risk_limits(const RiskLimits& limits) noexcept {
    risk_limits_ = limits;
    initialize_circuit_breakers(); // Reinitialize with new limits
}

inline void RiskControlSystem::calculate_portfolio_metrics() noexcept {
    // Update unrealized P&L and VaR for all instruments
    for (auto& risk : instrument_risks_) {
        if (risk.net_position != 0) {
            // Simple unrealized P&L calculation
            risk.unrealized_pnl = risk.market_value; // Simplified
            
            // Simple VaR calculation (would be more sophisticated in production)
            risk.value_at_risk = std::abs(risk.market_value) * 0.02; // 2% VaR assumption
        }
    }
}

inline bool RiskControlSystem::check_concentration_limits() noexcept {
    // Check concentration risk - simplified implementation
    double total_exposure = 0.0;
    for (const auto& risk : instrument_risks_) {
        total_exposure += std::abs(risk.market_value);
    }
    
    if (total_exposure == 0.0) return true;
    
    for (const auto& risk : instrument_risks_) {
        const double concentration = std::abs(risk.market_value) / total_exposure;
        if (concentration > risk_limits_.max_concentration_ratio) {
            trigger_circuit_breaker(CircuitBreakerType::CONCENTRATION_LIMIT,
                                   risk_limits_.max_concentration_ratio,
                                   concentration,
                                   "Concentration limit exceeded");
            return false;
        }
    }
    
    return true;
}

// Static asserts
static_assert(alignof(RiskControlSystem) == 64, "RiskControlSystem must be cache-aligned");

} // namespace trading
} // namespace hft