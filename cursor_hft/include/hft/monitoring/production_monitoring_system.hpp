#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"

namespace hft {
namespace monitoring {

using namespace hft::market_data;

/**
 * @brief Production monitoring and alerting system for HFT infrastructure
 * 
 * Provides comprehensive real-time monitoring and alerting:
 * - Ultra-low latency performance tracking across all components
 * - Real-time health status monitoring and reporting
 * - Configurable multi-level alerting with escalation
 * - Live operational dashboard with historical trending
 * - Resource utilization and capacity monitoring
 * - Component connectivity and status tracking
 * - Breach detection and notification system
 * 
 * Performance targets:
 * - Metric collection: <50ns per metric
 * - Alert processing: <100ns
 * - Dashboard update: <1μs
 * - Health check: <75ns
 * - Notification dispatch: <200ns
 */
class alignas(64) ProductionMonitoringSystem {
public:
    static constexpr size_t MAX_COMPONENTS = 16;        // Maximum monitored components
    static constexpr size_t MAX_METRICS = 64;           // Maximum metrics per component
    static constexpr size_t MAX_ALERTS = 1000;          // Maximum active alerts
    static constexpr size_t METRIC_HISTORY_SIZE = 10000; // Historical metric samples
    static constexpr size_t DASHBOARD_UPDATE_INTERVAL_NS = 1000000; // 1ms dashboard updates
    
    // Alert severity levels
    enum class AlertSeverity : uint8_t {
        INFO = 0,           // Informational
        WARNING = 1,        // Warning level
        CRITICAL = 2,       // Critical issue
        EMERGENCY = 3       // Emergency requiring immediate action
    };
    
    // Component health status
    enum class ComponentHealth : uint8_t {
        HEALTHY = 0,        // Component operating normally
        DEGRADED = 1,       // Performance degraded but functional
        UNHEALTHY = 2,      // Component experiencing issues
        FAILED = 3,         // Component failed
        UNKNOWN = 4         // Health status unknown
    };
    
    // Metric types for different monitoring categories
    enum class MetricType : uint8_t {
        LATENCY = 0,        // Latency measurements (ns)
        THROUGHPUT = 1,     // Throughput measurements (ops/sec)
        COUNT = 2,          // Count/counter metrics
        PERCENTAGE = 3,     // Percentage metrics (0-100)
        MEMORY = 4,         // Memory usage (bytes)
        RATIO = 5           // General ratio metrics
    };
    
    // Component types for monitoring
    enum class ComponentType : uint8_t {
        FEED_HANDLER = 0,
        ORDER_BOOK = 1,
        ORDER_LIFECYCLE = 2,
        POSITION_RECONCILIATION = 3,
        RISK_CONTROL = 4,
        STRATEGY_ENGINE = 5,
        VENUE_CONNECTOR = 6,
        MARKET_DATA = 7,
        MESSAGING = 8,
        MEMORY_POOL = 9,
        TIMER_SYSTEM = 10,
        NETWORK = 11,
        SYSTEM_RESOURCE = 12,
        CUSTOM = 15
    };
    
    // Real-time metric sample
    struct alignas(64) MetricSample {
        uint64_t timestamp_ns = 0;                      // Sample timestamp (8 bytes)
        uint64_t component_id = 0;                      // Component identifier (8 bytes)
        uint32_t metric_id = 0;                         // Metric identifier (4 bytes)
        MetricType type;                                // Metric type (1 byte)
        uint8_t _pad0[3];                              // Padding (3 bytes)
        double value = 0.0;                            // Metric value (8 bytes)
        double min_value = 0.0;                        // Min value in window (8 bytes)
        double max_value = 0.0;                        // Max value in window (8 bytes)
        double avg_value = 0.0;                        // Average value in window (8 bytes)
        uint32_t sample_count = 0;                     // Samples in current window (4 bytes)
        uint32_t _reserved = 0;                        // Reserved (4 bytes)
        // Total: 8+8+4+1+3+8+8+8+8+4+4 = 64 bytes
        
        MetricSample() noexcept : _pad0{} {}
    };
    static_assert(sizeof(MetricSample) == 64, "MetricSample must be 64 bytes");
    
    // Component health and status information
    struct alignas(64) ComponentStatus {
        uint64_t component_id = 0;                      // Component identifier (8 bytes)
        ComponentType type;                             // Component type (1 byte)
        ComponentHealth health = ComponentHealth::UNKNOWN; // Health status (1 byte)
        uint8_t _pad0[6];                              // Padding (6 bytes)
        uint64_t last_heartbeat_ns = 0;                // Last heartbeat timestamp (8 bytes)
        uint64_t startup_time_ns = 0;                   // Component startup time (8 bytes)
        uint64_t total_operations = 0;                  // Total operations processed (8 bytes)
        double cpu_usage_percent = 0.0;                // CPU usage percentage (8 bytes)
        double memory_usage_mb = 0.0;                  // Memory usage in MB (8 bytes)
        char component_name[16];                        // Component name (16 bytes)
        // Total: 8+1+1+6+8+8+8+8+8+16 = 72 bytes, pad to 128
        uint8_t _pad1[56];                             // Padding to 128 bytes
        
        ComponentStatus() noexcept : _pad0{}, _pad1{} { component_name[0] = '\0'; }
    };
    static_assert(sizeof(ComponentStatus) == 128, "ComponentStatus must be 128 bytes");
    
    // Alert configuration and state
    struct alignas(64) Alert {
        uint64_t alert_id = 0;                          // Unique alert ID (8 bytes)
        uint64_t component_id = 0;                      // Related component (8 bytes)
        uint64_t metric_id = 0;                         // Related metric (8 bytes)
        uint64_t trigger_time_ns = 0;                   // Alert trigger time (8 bytes)
        uint64_t last_trigger_ns = 0;                   // Last trigger time (8 bytes)
        AlertSeverity severity;                         // Alert severity (1 byte)
        bool active = false;                            // Currently active (1 byte)
        bool acknowledged = false;                      // Acknowledged by operator (1 byte)
        uint8_t _pad0[5];                              // Padding (5 bytes)
        double threshold_value = 0.0;                   // Alert threshold (8 bytes)
        double current_value = 0.0;                     // Current metric value (8 bytes)
        uint32_t trigger_count = 0;                     // Number of triggers (4 bytes)
        uint32_t _reserved = 0;                        // Reserved (4 bytes)
        // Total: 8+8+8+8+8+1+1+1+5+8+8+4+4 = 80 bytes, pad to 128
        uint8_t _pad1[48];                             // Padding to 128 bytes
        
        Alert() noexcept : _pad0{}, _pad1{} {}
    };
    static_assert(sizeof(Alert) == 128, "Alert must be 128 bytes");
    
    // Dashboard snapshot for real-time display
    struct DashboardSnapshot {
        uint64_t snapshot_time_ns = 0;
        uint64_t system_uptime_ns = 0;
        uint32_t total_components = 0;
        uint32_t healthy_components = 0;
        uint32_t degraded_components = 0;
        uint32_t failed_components = 0;
        uint32_t active_alerts = 0;
        uint32_t critical_alerts = 0;
        double avg_system_latency_ns = 0.0;
        double peak_system_latency_ns = 0.0;
        double total_throughput_ops_sec = 0.0;
        double memory_usage_percent = 0.0;
        double cpu_usage_percent = 0.0;
        uint64_t total_messages_processed = 0;
        uint64_t total_errors = 0;
    };
    
    // Alert configuration for thresholds
    struct AlertConfig {
        MetricType metric_type;
        double warning_threshold = 0.0;
        double critical_threshold = 0.0;
        double emergency_threshold = 0.0;
        uint32_t trigger_window_samples = 5;           // Number of samples to trigger
        bool enabled = true;
        char description[64];
        
        AlertConfig() noexcept { description[0] = '\0'; }
    };
    
    // Performance metrics for the monitoring system itself
    struct MonitoringMetrics {
        std::atomic<uint64_t> metrics_collected{0};
        std::atomic<uint64_t> alerts_triggered{0};
        std::atomic<uint64_t> alerts_resolved{0};
        std::atomic<uint64_t> dashboard_updates{0};
        std::atomic<uint64_t> total_monitoring_time_ns{0};
        std::atomic<uint64_t> total_alert_processing_time_ns{0};
        std::atomic<uint64_t> total_dashboard_update_time_ns{0};
    };
    
    /**
     * @brief Constructor with infrastructure dependencies
     */
    ProductionMonitoringSystem(
        hft::ObjectPool<MetricSample, 10000>& metric_pool,
        hft::SPSCRingBuffer<Alert, 1024>& alert_buffer
    ) noexcept;
    
    // No copy or move semantics
    ProductionMonitoringSystem(const ProductionMonitoringSystem&) = delete;
    ProductionMonitoringSystem& operator=(const ProductionMonitoringSystem&) = delete;
    
    /**
     * @brief Register a component for monitoring
     * @param component_name Human-readable component name
     * @param type Component type
     * @return Component ID for future metric submissions
     */
    [[nodiscard]] uint64_t register_component(
        const char* component_name,
        ComponentType type
    ) noexcept;
    
    /**
     * @brief Record a metric sample
     * @param component_id Component identifier
     * @param metric_id Metric identifier within component
     * @param type Metric type
     * @param value Metric value
     */
    void record_metric(
        uint64_t component_id,
        uint32_t metric_id,
        MetricType type,
        double value
    ) noexcept;
    
    /**
     * @brief Update component heartbeat
     * @param component_id Component identifier
     * @param health Current health status
     * @param cpu_usage CPU usage percentage
     * @param memory_usage Memory usage in MB
     */
    void update_component_heartbeat(
        uint64_t component_id,
        ComponentHealth health,
        double cpu_usage = 0.0,
        double memory_usage = 0.0
    ) noexcept;
    
    /**
     * @brief Configure alert thresholds for a metric
     * @param component_id Component identifier
     * @param metric_id Metric identifier
     * @param config Alert configuration
     */
    void configure_alert(
        uint64_t component_id,
        uint32_t metric_id,
        const AlertConfig& config
    ) noexcept;
    
    /**
     * @brief Process alerts and check thresholds
     * @return Number of new alerts triggered
     */
    [[nodiscard]] size_t process_alerts() noexcept;
    
    /**
     * @brief Acknowledge an active alert
     * @param alert_id Alert to acknowledge
     * @return true if alert acknowledged successfully
     */
    bool acknowledge_alert(uint64_t alert_id) noexcept;
    
    /**
     * @brief Generate dashboard snapshot
     * @return Current system status snapshot
     */
    [[nodiscard]] DashboardSnapshot generate_dashboard_snapshot() const noexcept;
    
    /**
     * @brief Get component status
     * @param component_id Component identifier
     * @return Component status information
     */
    [[nodiscard]] const ComponentStatus& get_component_status(uint64_t component_id) const noexcept;
    
    /**
     * @brief Get recent metric samples
     * @param component_id Component identifier
     * @param metric_id Metric identifier
     * @param max_samples Maximum number of samples to return
     * @return Vector of recent metric samples
     */
    [[nodiscard]] std::vector<MetricSample> get_metric_history(
        uint64_t component_id,
        uint32_t metric_id,
        size_t max_samples = 100
    ) const noexcept;
    
    /**
     * @brief Get active alerts
     * @param severity_filter Minimum severity level to include
     * @return Vector of active alerts
     */
    [[nodiscard]] std::vector<Alert> get_active_alerts(
        AlertSeverity severity_filter = AlertSeverity::INFO
    ) const noexcept;
    
    /**
     * @brief Get monitoring system performance metrics
     * @return Monitoring system metrics
     */
    [[nodiscard]] const MonitoringMetrics& get_monitoring_metrics() const noexcept { return metrics_; }
    
    /**
     * @brief Reset all metrics and alerts (maintenance operation)
     */
    void reset_monitoring_state() noexcept;

private:
    // Infrastructure references
    alignas(64) hft::ObjectPool<MetricSample, 10000>& metric_pool_;
    alignas(64) hft::SPSCRingBuffer<Alert, 1024>& alert_buffer_;
    alignas(64) hft::HFTTimer timer_;
    
    // Component tracking
    alignas(64) std::array<ComponentStatus, MAX_COMPONENTS> components_;
    alignas(64) std::atomic<size_t> component_count_;
    alignas(64) std::atomic<uint64_t> next_component_id_;
    
    // Metric storage and tracking
    alignas(64) std::array<std::array<MetricSample, MAX_METRICS>, MAX_COMPONENTS> latest_metrics_;
    alignas(64) std::array<MetricSample, METRIC_HISTORY_SIZE> metric_history_;
    alignas(64) std::atomic<size_t> metric_history_index_;
    
    // Alert management
    alignas(64) std::array<Alert, MAX_ALERTS> alerts_;
    alignas(64) std::array<std::array<AlertConfig, MAX_METRICS>, MAX_COMPONENTS> alert_configs_;
    alignas(64) std::atomic<size_t> alert_count_;
    alignas(64) std::atomic<uint64_t> next_alert_id_;
    
    // Dashboard and reporting
    alignas(64) std::atomic<uint64_t> last_dashboard_update_ns_;
    alignas(64) std::atomic<uint64_t> system_startup_time_ns_;
    
    // Performance tracking
    alignas(64) MonitoringMetrics metrics_;
    
    // Helper methods
    void check_metric_alerts(uint64_t component_id, uint32_t metric_id, const MetricSample& sample) noexcept;
    void trigger_alert(uint64_t component_id, uint32_t metric_id, AlertSeverity severity, 
                      double threshold, double current_value, const char* description) noexcept;
    void update_metric_statistics(MetricSample& sample, double new_value) noexcept;
    void add_metric_to_history(const MetricSample& sample) noexcept;
    
    [[nodiscard]] size_t get_component_index(uint64_t component_id) const noexcept;
    [[nodiscard]] bool is_component_healthy(const ComponentStatus& status) const noexcept;
    [[nodiscard]] double calculate_system_average_latency() const noexcept;
    [[nodiscard]] double calculate_total_throughput() const noexcept;
};

// Implementation

inline ProductionMonitoringSystem::ProductionMonitoringSystem(
    hft::ObjectPool<MetricSample, 10000>& metric_pool,
    hft::SPSCRingBuffer<Alert, 1024>& alert_buffer
) noexcept
    : metric_pool_(metric_pool),
      alert_buffer_(alert_buffer),
      timer_(),
      components_{},
      component_count_(0),
      next_component_id_(1),
      latest_metrics_{},
      metric_history_{},
      metric_history_index_(0),
      alerts_{},
      alert_configs_{},
      alert_count_(0),
      next_alert_id_(1),
      last_dashboard_update_ns_(0),
      system_startup_time_ns_(timer_.get_timestamp_ns()),
      metrics_{} {
    
    // Initialize component statuses
    for (auto& component : components_) {
        component.health = ComponentHealth::UNKNOWN;
    }
    
    // Initialize alert configurations with default values
    for (size_t comp = 0; comp < MAX_COMPONENTS; ++comp) {
        for (size_t metric = 0; metric < MAX_METRICS; ++metric) {
            auto& config = alert_configs_[comp][metric];
            config.enabled = false; // Disabled by default
            config.metric_type = MetricType::LATENCY;
            config.warning_threshold = 1000.0;   // 1μs default
            config.critical_threshold = 5000.0;  // 5μs default
            config.emergency_threshold = 10000.0; // 10μs default
            config.trigger_window_samples = 5;
        }
    }
}

inline uint64_t ProductionMonitoringSystem::register_component(
    const char* component_name,
    ComponentType type
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const size_t current_count = component_count_.load(std::memory_order_acquire);
    if (current_count >= MAX_COMPONENTS) {
        return 0; // No more component slots available
    }
    
    const uint64_t component_id = next_component_id_.fetch_add(1, std::memory_order_relaxed);
    const size_t component_index = current_count;
    
    auto& component = components_[component_index];
    component.component_id = component_id;
    component.type = type;
    component.health = ComponentHealth::HEALTHY;
    component.startup_time_ns = start_time;
    component.last_heartbeat_ns = start_time;
    component.total_operations = 0;
    component.cpu_usage_percent = 0.0;
    component.memory_usage_mb = 0.0;
    
    // Copy component name safely
    const size_t max_len = sizeof(component.component_name) - 1;
    size_t i = 0;
    while (i < max_len && component_name[i] != '\0') {
        component.component_name[i] = component_name[i];
        ++i;
    }
    component.component_name[i] = '\0';
    
    component_count_.store(current_count + 1, std::memory_order_release);
    
    return component_id;
}

inline void ProductionMonitoringSystem::record_metric(
    uint64_t component_id,
    uint32_t metric_id,
    MetricType type,
    double value
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const size_t component_index = get_component_index(component_id);
    if (component_index >= MAX_COMPONENTS || metric_id >= MAX_METRICS) {
        return;
    }
    
    // Update latest metric
    auto& metric = latest_metrics_[component_index][metric_id];
    metric.timestamp_ns = start_time;
    metric.component_id = component_id;
    metric.metric_id = metric_id;
    metric.type = type;
    
    // Update statistics
    update_metric_statistics(metric, value);
    
    // Add to history
    add_metric_to_history(metric);
    
    // Check for alert conditions
    check_metric_alerts(component_id, metric_id, metric);
    
    // Update performance metrics
    metrics_.metrics_collected.fetch_add(1, std::memory_order_relaxed);
    const auto collection_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_monitoring_time_ns.fetch_add(collection_time, std::memory_order_relaxed);
}

inline void ProductionMonitoringSystem::update_metric_statistics(
    MetricSample& sample,
    double new_value
) noexcept {
    sample.value = new_value;
    
    if (sample.sample_count == 0) {
        sample.min_value = new_value;
        sample.max_value = new_value;
        sample.avg_value = new_value;
        sample.sample_count = 1;
    } else {
        sample.min_value = std::min(sample.min_value, new_value);
        sample.max_value = std::max(sample.max_value, new_value);
        
        // Exponential moving average
        constexpr double alpha = 0.1;
        sample.avg_value = sample.avg_value * (1.0 - alpha) + new_value * alpha;
        
        ++sample.sample_count;
    }
}

inline void ProductionMonitoringSystem::add_metric_to_history(const MetricSample& sample) noexcept {
    const size_t index = metric_history_index_.fetch_add(1, std::memory_order_relaxed) % METRIC_HISTORY_SIZE;
    metric_history_[index] = sample;
}

inline void ProductionMonitoringSystem::update_component_heartbeat(
    uint64_t component_id,
    ComponentHealth health,
    double cpu_usage,
    double memory_usage
) noexcept {
    const size_t component_index = get_component_index(component_id);
    if (component_index >= MAX_COMPONENTS) return;
    
    auto& component = components_[component_index];
    component.health = health;
    component.last_heartbeat_ns = timer_.get_timestamp_ns();
    component.cpu_usage_percent = cpu_usage;
    component.memory_usage_mb = memory_usage;
    ++component.total_operations;
}

inline void ProductionMonitoringSystem::configure_alert(
    uint64_t component_id,
    uint32_t metric_id,
    const AlertConfig& config
) noexcept {
    const size_t component_index = get_component_index(component_id);
    if (component_index >= MAX_COMPONENTS || metric_id >= MAX_METRICS) return;
    
    alert_configs_[component_index][metric_id] = config;
}

inline void ProductionMonitoringSystem::check_metric_alerts(
    uint64_t component_id,
    uint32_t metric_id,
    const MetricSample& sample
) noexcept {
    const size_t component_index = get_component_index(component_id);
    if (component_index >= MAX_COMPONENTS || metric_id >= MAX_METRICS) return;
    
    const auto& config = alert_configs_[component_index][metric_id];
    if (!config.enabled) return;
    
    const double value = sample.avg_value; // Use average for stability
    
    // Check emergency threshold first
    if (value >= config.emergency_threshold) {
        trigger_alert(component_id, metric_id, AlertSeverity::EMERGENCY,
                     config.emergency_threshold, value, "Emergency threshold exceeded");
    } else if (value >= config.critical_threshold) {
        trigger_alert(component_id, metric_id, AlertSeverity::CRITICAL,
                     config.critical_threshold, value, "Critical threshold exceeded");
    } else if (value >= config.warning_threshold) {
        trigger_alert(component_id, metric_id, AlertSeverity::WARNING,
                     config.warning_threshold, value, "Warning threshold exceeded");
    }
}

inline void ProductionMonitoringSystem::trigger_alert(
    uint64_t component_id,
    uint32_t metric_id,
    AlertSeverity severity,
    double threshold,
    double current_value,
    const char* description
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const size_t alert_count = alert_count_.load(std::memory_order_acquire);
    if (alert_count >= MAX_ALERTS) return; // No space for new alerts
    
    const size_t alert_index = alert_count;
    const uint64_t alert_id = next_alert_id_.fetch_add(1, std::memory_order_relaxed);
    
    auto& alert = alerts_[alert_index];
    alert.alert_id = alert_id;
    alert.component_id = component_id;
    alert.metric_id = metric_id;
    alert.trigger_time_ns = start_time;
    alert.last_trigger_ns = start_time;
    alert.severity = severity;
    alert.active = true;
    alert.acknowledged = false;
    alert.threshold_value = threshold;
    alert.current_value = current_value;
    alert.trigger_count = 1;
    
    alert_count_.store(alert_count + 1, std::memory_order_release);
    
    // Try to send alert notification
    alert_buffer_.try_push(alert);
    
    // Update performance metrics
    metrics_.alerts_triggered.fetch_add(1, std::memory_order_relaxed);
    const auto processing_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_alert_processing_time_ns.fetch_add(processing_time, std::memory_order_relaxed);
}

inline size_t ProductionMonitoringSystem::process_alerts() noexcept {
    size_t new_alerts = 0;
    
    // Process existing alerts to check if they should be resolved
    const size_t alert_count = alert_count_.load(std::memory_order_acquire);
    for (size_t i = 0; i < alert_count; ++i) {
        auto& alert = alerts_[i];
        if (!alert.active) continue;
        
        // Check if alert condition is still true
        const size_t component_index = get_component_index(alert.component_id);
        if (component_index < MAX_COMPONENTS && alert.metric_id < MAX_METRICS) {
            const auto& metric = latest_metrics_[component_index][alert.metric_id];
            const auto& config = alert_configs_[component_index][alert.metric_id];
            
            bool should_resolve = false;
            switch (alert.severity) {
                case AlertSeverity::EMERGENCY:
                    should_resolve = (metric.avg_value < config.emergency_threshold * 0.9); // 10% hysteresis
                    break;
                case AlertSeverity::CRITICAL:
                    should_resolve = (metric.avg_value < config.critical_threshold * 0.9);
                    break;
                case AlertSeverity::WARNING:
                    should_resolve = (metric.avg_value < config.warning_threshold * 0.9);
                    break;
                default:
                    should_resolve = true;
                    break;
            }
            
            if (should_resolve) {
                alert.active = false;
                metrics_.alerts_resolved.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    
    return new_alerts;
}

inline bool ProductionMonitoringSystem::acknowledge_alert(uint64_t alert_id) noexcept {
    const size_t alert_count = alert_count_.load(std::memory_order_acquire);
    
    for (size_t i = 0; i < alert_count; ++i) {
        auto& alert = alerts_[i];
        if (alert.alert_id == alert_id && alert.active) {
            alert.acknowledged = true;
            return true;
        }
    }
    
    return false;
}

inline ProductionMonitoringSystem::DashboardSnapshot 
ProductionMonitoringSystem::generate_dashboard_snapshot() const noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    DashboardSnapshot snapshot;
    snapshot.snapshot_time_ns = start_time;
    snapshot.system_uptime_ns = start_time - system_startup_time_ns_.load();
    
    const size_t component_count = component_count_.load(std::memory_order_acquire);
    snapshot.total_components = static_cast<uint32_t>(component_count);
    
    // Count component health status
    for (size_t i = 0; i < component_count; ++i) {
        const auto& component = components_[i];
        switch (component.health) {
            case ComponentHealth::HEALTHY:
                ++snapshot.healthy_components;
                break;
            case ComponentHealth::DEGRADED:
                ++snapshot.degraded_components;
                break;
            case ComponentHealth::UNHEALTHY:
            case ComponentHealth::FAILED:
                ++snapshot.failed_components;
                break;
            default:
                break;
        }
        
        snapshot.memory_usage_percent += component.memory_usage_mb;
        snapshot.cpu_usage_percent += component.cpu_usage_percent;
        snapshot.total_messages_processed += component.total_operations;
    }
    
    if (component_count > 0) {
        snapshot.cpu_usage_percent /= component_count;
    }
    
    // Count active alerts
    const size_t alert_count = alert_count_.load(std::memory_order_acquire);
    for (size_t i = 0; i < alert_count; ++i) {
        const auto& alert = alerts_[i];
        if (alert.active) {
            ++snapshot.active_alerts;
            if (alert.severity >= AlertSeverity::CRITICAL) {
                ++snapshot.critical_alerts;
            }
        }
    }
    
    // Calculate system-wide metrics
    snapshot.avg_system_latency_ns = calculate_system_average_latency();
    snapshot.total_throughput_ops_sec = calculate_total_throughput();
    
    return snapshot;
}

inline double ProductionMonitoringSystem::calculate_system_average_latency() const noexcept {
    double total_latency = 0.0;
    size_t latency_metric_count = 0;
    
    const size_t component_count = component_count_.load(std::memory_order_acquire);
    for (size_t comp = 0; comp < component_count; ++comp) {
        for (size_t metric = 0; metric < MAX_METRICS; ++metric) {
            const auto& sample = latest_metrics_[comp][metric];
            if (sample.type == MetricType::LATENCY && sample.sample_count > 0) {
                total_latency += sample.avg_value;
                ++latency_metric_count;
            }
        }
    }
    
    return (latency_metric_count > 0) ? (total_latency / latency_metric_count) : 0.0;
}

inline double ProductionMonitoringSystem::calculate_total_throughput() const noexcept {
    double total_throughput = 0.0;
    
    const size_t component_count = component_count_.load(std::memory_order_acquire);
    for (size_t comp = 0; comp < component_count; ++comp) {
        for (size_t metric = 0; metric < MAX_METRICS; ++metric) {
            const auto& sample = latest_metrics_[comp][metric];
            if (sample.type == MetricType::THROUGHPUT && sample.sample_count > 0) {
                total_throughput += sample.avg_value;
            }
        }
    }
    
    return total_throughput;
}

// Getter implementations
inline const ProductionMonitoringSystem::ComponentStatus& 
ProductionMonitoringSystem::get_component_status(uint64_t component_id) const noexcept {
    const size_t component_index = get_component_index(component_id);
    if (component_index < MAX_COMPONENTS) {
        return components_[component_index];
    }
    return components_[0]; // Fallback
}

inline std::vector<ProductionMonitoringSystem::MetricSample> 
ProductionMonitoringSystem::get_metric_history(
    uint64_t component_id,
    uint32_t metric_id,
    size_t max_samples
) const noexcept {
    std::vector<MetricSample> history;
    history.reserve(max_samples);
    
    const size_t history_size = std::min(
        metric_history_index_.load(std::memory_order_acquire), 
        METRIC_HISTORY_SIZE
    );
    
    for (size_t i = 0; i < history_size && history.size() < max_samples; ++i) {
        const auto& sample = metric_history_[i];
        if (sample.component_id == component_id && sample.metric_id == metric_id) {
            history.push_back(sample);
        }
    }
    
    return history;
}

inline std::vector<ProductionMonitoringSystem::Alert> 
ProductionMonitoringSystem::get_active_alerts(AlertSeverity severity_filter) const noexcept {
    std::vector<Alert> active_alerts;
    
    const size_t alert_count = alert_count_.load(std::memory_order_acquire);
    for (size_t i = 0; i < alert_count; ++i) {
        const auto& alert = alerts_[i];
        if (alert.active && alert.severity >= severity_filter) {
            active_alerts.push_back(alert);
        }
    }
    
    return active_alerts;
}

inline void ProductionMonitoringSystem::reset_monitoring_state() noexcept {
    // Reset component count
    component_count_.store(0, std::memory_order_release);
    next_component_id_.store(1, std::memory_order_release);
    
    // Reset alerts
    alert_count_.store(0, std::memory_order_release);
    next_alert_id_.store(1, std::memory_order_release);
    
    // Reset metrics history
    metric_history_index_.store(0, std::memory_order_release);
    
    // Reset startup time
    system_startup_time_ns_.store(timer_.get_timestamp_ns(), std::memory_order_release);
    
    // Clear components and metrics
    for (auto& component : components_) {
        component.health = ComponentHealth::UNKNOWN;
        component.component_name[0] = '\0';
    }
}

inline size_t ProductionMonitoringSystem::get_component_index(uint64_t component_id) const noexcept {
    const size_t component_count = component_count_.load(std::memory_order_acquire);
    for (size_t i = 0; i < component_count; ++i) {
        if (components_[i].component_id == component_id) {
            return i;
        }
    }
    return MAX_COMPONENTS; // Not found
}

inline bool ProductionMonitoringSystem::is_component_healthy(const ComponentStatus& status) const noexcept {
    const uint64_t current_time = timer_.get_timestamp_ns();
    const uint64_t heartbeat_timeout = 5000000000ULL; // 5 seconds
    
    return (status.health == ComponentHealth::HEALTHY) &&
           ((current_time - status.last_heartbeat_ns) < heartbeat_timeout);
}

// Static asserts
static_assert(alignof(ProductionMonitoringSystem) == 64, "ProductionMonitoringSystem must be cache-aligned");

} // namespace monitoring
} // namespace hft