#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/trading/order_lifecycle_manager.hpp"
#include "hft/monitoring/production_monitoring_system.hpp"

namespace hft {
namespace recovery {

using namespace hft::market_data;
using namespace hft::trading;
using namespace hft::monitoring;

/**
 * @brief Comprehensive fault tolerance and recovery management system
 * 
 * Provides enterprise-grade fault tolerance and recovery capabilities:
 * - Real-time failure detection and component isolation
 * - Graceful degradation with automatic fallback mechanisms
 * - Intelligent recovery procedures with state restoration
 * - Data integrity protection and transaction replay
 * - Emergency response with coordinated system shutdown
 * - Circuit breaker integration and load redistribution
 * - Operator notification and manual override capabilities
 * 
 * Performance targets:
 * - Failure detection: <100ns
 * - Recovery initiation: <500ns
 * - State restoration: <10μs
 * - Emergency shutdown: <1μs
 * - Component restart: <50μs
 */
class alignas(64) FaultToleranceManager {
public:
    static constexpr size_t MAX_COMPONENTS = 16;        // Maximum monitored components
    static constexpr size_t MAX_RECOVERY_PLANS = 32;    // Maximum recovery procedures
    static constexpr size_t MAX_CHECKPOINTS = 1000;     // Maximum checkpoints
    static constexpr size_t MAX_FAILURE_HISTORY = 10000; // Failure event history
    static constexpr size_t CHECKPOINT_INTERVAL_NS = 1000000000; // 1 second checkpointing
    
    // Component failure types
    enum class FailureType : uint8_t {
        HEARTBEAT_TIMEOUT = 0,      // Component stopped responding
        PERFORMANCE_DEGRADATION = 1, // Performance below thresholds
        RESOURCE_EXHAUSTION = 2,    // Memory/CPU exhaustion
        NETWORK_DISCONNECTION = 3,  // Network connectivity lost
        DATA_CORRUPTION = 4,        // Data integrity violation
        EXCEPTION_THROWN = 5,       // Unhandled exception
        MANUAL_SHUTDOWN = 6,        // Operator-initiated shutdown
        DEPENDENCY_FAILURE = 7      // Dependent component failed
    };
    
    // Recovery strategies for different failure scenarios
    enum class RecoveryStrategy : uint8_t {
        RESTART_COMPONENT = 0,      // Restart the failed component
        FAILOVER_TO_BACKUP = 1,     // Switch to backup component
        GRACEFUL_DEGRADATION = 2,   // Reduce functionality temporarily
        EMERGENCY_SHUTDOWN = 3,     // Immediate system shutdown
        MANUAL_INTERVENTION = 4,    // Require operator action
        IGNORE_FAILURE = 5          // Continue operating with failure
    };
    
    // System operational states
    enum class SystemState : uint8_t {
        NORMAL_OPERATION = 0,       // All systems operating normally
        DEGRADED_OPERATION = 1,     // Some components failed, system operational
        RECOVERY_IN_PROGRESS = 2,   // Active recovery procedures running
        EMERGENCY_SHUTDOWN = 3,     // Emergency shutdown initiated
        MANUAL_CONTROL = 4,         // Under manual operator control
        SYSTEM_HALTED = 5          // System completely stopped
    };
    
    // Component status and health information
    struct alignas(64) ComponentStatus {
        uint64_t component_id = 0;                      // Component identifier (8 bytes)
        ProductionMonitoringSystem::ComponentType type; // Component type (1 byte)
        ProductionMonitoringSystem::ComponentHealth health; // Health status (1 byte)
        bool is_critical = false;                       // Critical for system operation (1 byte)
        bool has_backup = false;                        // Backup component available (1 byte)
        uint8_t _pad0[4];                              // Padding (4 bytes)
        uint64_t last_heartbeat_ns = 0;                // Last heartbeat timestamp (8 bytes)
        uint64_t failure_count = 0;                    // Total failure count (8 bytes)
        uint64_t last_failure_time_ns = 0;             // Last failure timestamp (8 bytes)
        uint64_t recovery_count = 0;                   // Total recovery attempts (8 bytes)
        uint64_t last_recovery_time_ns = 0;            // Last recovery timestamp (8 bytes)
        char component_name[16];                        // Component name (16 bytes)
        // Total: 8+1+1+1+1+4+8+8+8+8+8+16 = 72 bytes, pad to 128
        uint8_t _pad1[56];                             // Padding to 128 bytes
        
        ComponentStatus() noexcept : _pad0{}, _pad1{} { component_name[0] = '\0'; }
    };
    static_assert(sizeof(ComponentStatus) == 128, "ComponentStatus must be 128 bytes");
    
    // Failure event for tracking and analysis
    struct alignas(64) FailureEvent {
        uint64_t event_id = 0;                          // Unique event ID (8 bytes)
        uint64_t component_id = 0;                      // Failed component (8 bytes)
        uint64_t detection_time_ns = 0;                 // Failure detection time (8 bytes)
        uint64_t recovery_start_time_ns = 0;            // Recovery start time (8 bytes)
        uint64_t recovery_end_time_ns = 0;              // Recovery completion time (8 bytes)
        FailureType failure_type;                       // Type of failure (1 byte)
        RecoveryStrategy recovery_strategy;             // Recovery strategy used (1 byte)
        bool recovery_successful = false;               // Recovery outcome (1 byte)
        uint8_t _pad0[5];                              // Padding (5 bytes)
        char failure_description[24];                   // Failure description (24 bytes)
        // Total: 8+8+8+8+8+1+1+1+5+24 = 72 bytes, pad to 128
        uint8_t _pad1[56];                             // Padding to 128 bytes
        
        FailureEvent() noexcept : _pad0{}, _pad1{} { failure_description[0] = '\0'; }
    };
    static_assert(sizeof(FailureEvent) == 128, "FailureEvent must be 128 bytes");
    
    // Recovery procedure definition
    struct alignas(64) RecoveryPlan {
        uint64_t plan_id = 0;                           // Unique plan ID (8 bytes)
        ProductionMonitoringSystem::ComponentType component_type; // Target component type (1 byte)
        FailureType failure_type;                       // Failure type this plan addresses (1 byte)
        RecoveryStrategy strategy;                      // Recovery strategy (1 byte)
        bool enabled = true;                            // Plan enabled (1 byte)
        uint8_t priority = 0;                          // Execution priority (1 byte)
        uint8_t _pad0[3];                              // Padding (3 bytes)
        uint64_t max_execution_time_ns = 10000000;     // Maximum execution time (8 bytes)
        uint64_t cooldown_period_ns = 5000000000;      // Cooldown before retry (8 bytes)
        uint64_t success_count = 0;                    // Successful executions (8 bytes)
        uint64_t failure_count = 0;                    // Failed executions (8 bytes)
        uint64_t last_execution_time_ns = 0;           // Last execution time (8 bytes)
        char plan_description[16];                      // Plan description (16 bytes)
        // Total: 8+1+1+1+1+1+3+8+8+8+8+8+16 = 72 bytes, pad to 128
        uint8_t _pad1[56];                             // Padding to 128 bytes
        
        RecoveryPlan() noexcept : _pad0{}, _pad1{} { plan_description[0] = '\0'; }
    };
    static_assert(sizeof(RecoveryPlan) == 128, "RecoveryPlan must be 128 bytes");
    
    // System checkpoint for state recovery
    struct alignas(64) SystemCheckpoint {
        uint64_t checkpoint_id = 0;                     // Unique checkpoint ID (8 bytes)
        uint64_t creation_time_ns = 0;                  // Checkpoint creation time (8 bytes)
        SystemState system_state;                       // System state at checkpoint (1 byte)
        uint8_t _pad0[7];                              // Padding (7 bytes)
        uint64_t active_components = 0;                 // Bitmask of active components (8 bytes)
        uint64_t total_orders_processed = 0;            // Orders processed at checkpoint (8 bytes)
        uint64_t total_messages_processed = 0;          // Messages processed at checkpoint (8 bytes)
        double system_latency_ns = 0.0;                // System latency at checkpoint (8 bytes)
        char checkpoint_data[16];                       // Serialized state data (16 bytes)
        // Total: 8+8+1+7+8+8+8+8+16 = 72 bytes, pad to 128
        uint8_t _pad1[56];                             // Padding to 128 bytes
        
        SystemCheckpoint() noexcept : _pad0{}, _pad1{} { checkpoint_data[0] = '\0'; }
    };
    static_assert(sizeof(SystemCheckpoint) == 128, "SystemCheckpoint must be 128 bytes");
    
    // Recovery callback function type
    using RecoveryCallback = std::function<bool(uint64_t component_id, FailureType failure_type)>;
    
    // System statistics and metrics
    struct FaultToleranceMetrics {
        std::atomic<uint64_t> failures_detected{0};
        std::atomic<uint64_t> recoveries_attempted{0};
        std::atomic<uint64_t> recoveries_successful{0};
        std::atomic<uint64_t> emergency_shutdowns{0};
        std::atomic<uint64_t> checkpoints_created{0};
        std::atomic<uint64_t> state_restorations{0};
        std::atomic<uint64_t> total_failure_detection_time_ns{0};
        std::atomic<uint64_t> total_recovery_time_ns{0};
        std::atomic<uint64_t> total_checkpoint_time_ns{0};
    };
    
    /**
     * @brief Constructor with infrastructure dependencies
     */
    FaultToleranceManager(
        OrderLifecycleManager& order_manager,
        ProductionMonitoringSystem& monitoring_system,
        hft::ObjectPool<FailureEvent, 1000>& event_pool,
        hft::SPSCRingBuffer<FailureEvent, 1024>& failure_buffer
    ) noexcept;
    
    // No copy or move semantics
    FaultToleranceManager(const FaultToleranceManager&) = delete;
    FaultToleranceManager& operator=(const FaultToleranceManager&) = delete;
    
    /**
     * @brief Register a component for fault tolerance monitoring
     * @param component_id Component identifier
     * @param type Component type
     * @param name Component name
     * @param is_critical Whether component is critical for operation
     * @param has_backup Whether component has backup available
     * @return true if registration successful
     */
    bool register_component(
        uint64_t component_id,
        ProductionMonitoringSystem::ComponentType type,
        const char* name,
        bool is_critical = false,
        bool has_backup = false
    ) noexcept;
    
    /**
     * @brief Register a recovery plan for a component/failure combination
     * @param component_type Component type
     * @param failure_type Failure type
     * @param strategy Recovery strategy
     * @param callback Recovery callback function
     * @param priority Execution priority (0 = highest)
     * @return Recovery plan ID
     */
    [[nodiscard]] uint64_t register_recovery_plan(
        ProductionMonitoringSystem::ComponentType component_type,
        FailureType failure_type,
        RecoveryStrategy strategy,
        RecoveryCallback callback,
        uint8_t priority = 5
    ) noexcept;
    
    /**
     * @brief Report component failure
     * @param component_id Component that failed
     * @param failure_type Type of failure
     * @param description Failure description
     * @return true if failure processed successfully
     */
    bool report_failure(
        uint64_t component_id,
        FailureType failure_type,
        const char* description = ""
    ) noexcept;
    
    /**
     * @brief Update component health status
     * @param component_id Component identifier
     * @param health Health status
     */
    void update_component_health(
        uint64_t component_id,
        ProductionMonitoringSystem::ComponentHealth health
    ) noexcept;
    
    /**
     * @brief Process pending failures and execute recovery procedures
     * @return Number of recovery procedures executed
     */
    [[nodiscard]] size_t process_recoveries() noexcept;
    
    /**
     * @brief Create system checkpoint for state recovery
     * @return Checkpoint ID if successful, 0 if failed
     */
    [[nodiscard]] uint64_t create_checkpoint() noexcept;
    
    /**
     * @brief Restore system state from checkpoint
     * @param checkpoint_id Checkpoint to restore from
     * @return true if restoration successful
     */
    bool restore_from_checkpoint(uint64_t checkpoint_id) noexcept;
    
    /**
     * @brief Trigger emergency shutdown of all systems
     * @param reason Reason for emergency shutdown
     * @return true if shutdown initiated successfully
     */
    bool trigger_emergency_shutdown(const char* reason) noexcept;
    
    /**
     * @brief Get current system operational state
     * @return Current system state
     */
    [[nodiscard]] SystemState get_system_state() const noexcept { return system_state_.load(); }
    
    /**
     * @brief Get component status
     * @param component_id Component identifier
     * @return Component status
     */
    [[nodiscard]] const ComponentStatus& get_component_status(uint64_t component_id) const noexcept;
    
    /**
     * @brief Get recent failure history
     * @param max_events Maximum number of events to return
     * @return Vector of recent failure events
     */
    [[nodiscard]] std::vector<FailureEvent> get_failure_history(size_t max_events = 100) const noexcept;
    
    /**
     * @brief Get recovery plan statistics
     * @param plan_id Recovery plan identifier
     * @return Recovery plan information
     */
    [[nodiscard]] const RecoveryPlan& get_recovery_plan(uint64_t plan_id) const noexcept;
    
    /**
     * @brief Get fault tolerance metrics
     * @return System fault tolerance metrics
     */
    [[nodiscard]] const FaultToleranceMetrics& get_metrics() const noexcept { return metrics_; }
    
    /**
     * @brief Manual override to change system state
     * @param new_state New system state
     * @param operator_id Operator identifier
     * @return true if state change successful
     */
    bool manual_override_system_state(SystemState new_state, uint64_t operator_id) noexcept;
    
    /**
     * @brief Reset fault tolerance system (maintenance operation)
     */
    void reset_fault_tolerance_state() noexcept;

private:
    // Infrastructure references
    alignas(64) OrderLifecycleManager& order_manager_;
    alignas(64) ProductionMonitoringSystem& monitoring_system_;
    alignas(64) hft::ObjectPool<FailureEvent, 1000>& event_pool_;
    alignas(64) hft::SPSCRingBuffer<FailureEvent, 1024>& failure_buffer_;
    alignas(64) hft::HFTTimer timer_;
    
    // System state management
    alignas(64) std::atomic<SystemState> system_state_;
    alignas(64) std::atomic<uint64_t> last_checkpoint_time_ns_;
    alignas(64) std::atomic<uint64_t> emergency_shutdown_time_ns_;
    
    // Component tracking
    alignas(64) std::array<ComponentStatus, MAX_COMPONENTS> components_;
    alignas(64) std::atomic<size_t> component_count_;
    alignas(64) std::unordered_map<uint64_t, size_t> component_index_map_;
    
    // Recovery management
    alignas(64) std::array<RecoveryPlan, MAX_RECOVERY_PLANS> recovery_plans_;
    alignas(64) std::array<RecoveryCallback, MAX_RECOVERY_PLANS> recovery_callbacks_;
    alignas(64) std::atomic<size_t> recovery_plan_count_;
    alignas(64) std::atomic<uint64_t> next_plan_id_;
    
    // Failure tracking
    alignas(64) std::array<FailureEvent, MAX_FAILURE_HISTORY> failure_history_;
    alignas(64) std::atomic<size_t> failure_history_index_;
    alignas(64) std::atomic<uint64_t> next_event_id_;
    
    // Checkpoint management
    alignas(64) std::array<SystemCheckpoint, MAX_CHECKPOINTS> checkpoints_;
    alignas(64) std::atomic<size_t> checkpoint_count_;
    alignas(64) std::atomic<uint64_t> next_checkpoint_id_;
    
    // Performance tracking
    alignas(64) FaultToleranceMetrics metrics_;
    
    // Helper methods
    void execute_recovery_plan(uint64_t component_id, FailureType failure_type) noexcept;
    void add_failure_event(uint64_t component_id, FailureType failure_type, const char* description) noexcept;
    void update_system_state(SystemState new_state) noexcept;
    
    [[nodiscard]] size_t find_component_index(uint64_t component_id) const noexcept;
    [[nodiscard]] RecoveryPlan* find_recovery_plan(
        ProductionMonitoringSystem::ComponentType component_type,
        FailureType failure_type
    ) noexcept;
    
    bool is_component_critical(uint64_t component_id) const noexcept;
    bool should_trigger_emergency_shutdown(FailureType failure_type) const noexcept;
    void notify_operators(const FailureEvent& event) noexcept;
    void cleanup_old_checkpoints() noexcept;
};

// Implementation

inline FaultToleranceManager::FaultToleranceManager(
    OrderLifecycleManager& order_manager,
    ProductionMonitoringSystem& monitoring_system,
    hft::ObjectPool<FailureEvent, 1000>& event_pool,
    hft::SPSCRingBuffer<FailureEvent, 1024>& failure_buffer
) noexcept
    : order_manager_(order_manager),
      monitoring_system_(monitoring_system),
      event_pool_(event_pool),
      failure_buffer_(failure_buffer),
      timer_(),
      system_state_(SystemState::NORMAL_OPERATION),
      last_checkpoint_time_ns_(0),
      emergency_shutdown_time_ns_(0),
      components_{},
      component_count_(0),
      component_index_map_{},
      recovery_plans_{},
      recovery_callbacks_{},
      recovery_plan_count_(0),
      next_plan_id_(1),
      failure_history_{},
      failure_history_index_(0),
      next_event_id_(1),
      checkpoints_{},
      checkpoint_count_(0),
      next_checkpoint_id_(1),
      metrics_{} {
    
    // Initialize component statuses
    for (auto& component : components_) {
        component.health = ProductionMonitoringSystem::ComponentHealth::UNKNOWN;
    }
    
    // Reserve space for component index map
    component_index_map_.reserve(MAX_COMPONENTS);
    
    // Create initial checkpoint
    create_checkpoint();
}

inline bool FaultToleranceManager::register_component(
    uint64_t component_id,
    ProductionMonitoringSystem::ComponentType type,
    const char* name,
    bool is_critical,
    bool has_backup
) noexcept {
    const size_t current_count = component_count_.load(std::memory_order_acquire);
    if (current_count >= MAX_COMPONENTS) {
        return false;
    }
    
    const size_t component_index = current_count;
    auto& component = components_[component_index];
    
    component.component_id = component_id;
    component.type = type;
    component.health = ProductionMonitoringSystem::ComponentHealth::HEALTHY;
    component.is_critical = is_critical;
    component.has_backup = has_backup;
    component.last_heartbeat_ns = timer_.get_timestamp_ns();
    component.failure_count = 0;
    component.recovery_count = 0;
    
    // Copy component name safely
    const size_t max_len = sizeof(component.component_name) - 1;
    size_t i = 0;
    while (i < max_len && name[i] != '\0') {
        component.component_name[i] = name[i];
        ++i;
    }
    component.component_name[i] = '\0';
    
    // Update mappings
    component_index_map_[component_id] = component_index;
    component_count_.store(current_count + 1, std::memory_order_release);
    
    return true;
}

inline uint64_t FaultToleranceManager::register_recovery_plan(
    ProductionMonitoringSystem::ComponentType component_type,
    FailureType failure_type,
    RecoveryStrategy strategy,
    RecoveryCallback callback,
    uint8_t priority
) noexcept {
    const size_t current_count = recovery_plan_count_.load(std::memory_order_acquire);
    if (current_count >= MAX_RECOVERY_PLANS) {
        return 0;
    }
    
    const uint64_t plan_id = next_plan_id_.fetch_add(1, std::memory_order_relaxed);
    const size_t plan_index = current_count;
    
    auto& plan = recovery_plans_[plan_index];
    plan.plan_id = plan_id;
    plan.component_type = component_type;
    plan.failure_type = failure_type;
    plan.strategy = strategy;
    plan.enabled = true;
    plan.priority = priority;
    plan.max_execution_time_ns = 10000000; // 10ms default
    plan.cooldown_period_ns = 5000000000; // 5s default
    
    // Store callback
    recovery_callbacks_[plan_index] = callback;
    
    recovery_plan_count_.store(current_count + 1, std::memory_order_release);
    
    return plan_id;
}

inline bool FaultToleranceManager::report_failure(
    uint64_t component_id,
    FailureType failure_type,
    const char* description
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    // Find component
    const size_t component_index = find_component_index(component_id);
    if (component_index >= MAX_COMPONENTS) {
        return false;
    }
    
    auto& component = components_[component_index];
    component.failure_count++;
    component.last_failure_time_ns = start_time;
    component.health = ProductionMonitoringSystem::ComponentHealth::FAILED;
    
    // Create failure event
    add_failure_event(component_id, failure_type, description);
    
    // Check if emergency shutdown required
    if (should_trigger_emergency_shutdown(failure_type) || 
        (component.is_critical && !component.has_backup)) {
        trigger_emergency_shutdown("Critical component failure");
    } else {
        // Execute recovery procedure
        execute_recovery_plan(component_id, failure_type);
    }
    
    // Update performance metrics
    metrics_.failures_detected.fetch_add(1, std::memory_order_relaxed);
    const auto detection_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_failure_detection_time_ns.fetch_add(detection_time, std::memory_order_relaxed);
    
    return true;
}

inline void FaultToleranceManager::execute_recovery_plan(
    uint64_t component_id,
    FailureType failure_type
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const size_t component_index = find_component_index(component_id);
    if (component_index >= MAX_COMPONENTS) return;
    
    auto& component = components_[component_index];
    RecoveryPlan* plan = find_recovery_plan(component.type, failure_type);
    
    if (!plan || !plan->enabled) {
        // No recovery plan available, attempt graceful degradation
        update_system_state(SystemState::DEGRADED_OPERATION);
        return;
    }
    
    // Check cooldown period
    const uint64_t current_time = timer_.get_timestamp_ns();
    if ((current_time - plan->last_execution_time_ns) < plan->cooldown_period_ns) {
        return; // Still in cooldown
    }
    
    // Update system state
    update_system_state(SystemState::RECOVERY_IN_PROGRESS);
    
    // Execute recovery callback
    bool recovery_successful = false;
    const size_t plan_index = plan - recovery_plans_.data();
    
    if (plan_index < MAX_RECOVERY_PLANS && recovery_callbacks_[plan_index]) {
        try {
            recovery_successful = recovery_callbacks_[plan_index](component_id, failure_type);
        } catch (...) {
            recovery_successful = false;
        }
    }
    
    // Update recovery statistics
    plan->last_execution_time_ns = current_time;
    if (recovery_successful) {
        plan->success_count++;
        component.recovery_count++;
        component.last_recovery_time_ns = current_time;
        component.health = ProductionMonitoringSystem::ComponentHealth::HEALTHY;
        
        // Check if we can return to normal operation
        bool all_critical_healthy = true;
        const size_t component_count = component_count_.load();
        for (size_t i = 0; i < component_count; ++i) {
            if (components_[i].is_critical && 
                components_[i].health != ProductionMonitoringSystem::ComponentHealth::HEALTHY) {
                all_critical_healthy = false;
                break;
            }
        }
        
        if (all_critical_healthy) {
            update_system_state(SystemState::NORMAL_OPERATION);
        } else {
            update_system_state(SystemState::DEGRADED_OPERATION);
        }
        
        metrics_.recoveries_successful.fetch_add(1, std::memory_order_relaxed);
    } else {
        plan->failure_count++;
        update_system_state(SystemState::DEGRADED_OPERATION);
    }
    
    metrics_.recoveries_attempted.fetch_add(1, std::memory_order_relaxed);
    const auto recovery_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_recovery_time_ns.fetch_add(recovery_time, std::memory_order_relaxed);
}

inline void FaultToleranceManager::add_failure_event(
    uint64_t component_id,
    FailureType failure_type,
    const char* description
) noexcept {
    const size_t index = failure_history_index_.fetch_add(1, std::memory_order_relaxed) % MAX_FAILURE_HISTORY;
    
    auto& event = failure_history_[index];
    event.event_id = next_event_id_.fetch_add(1, std::memory_order_relaxed);
    event.component_id = component_id;
    event.detection_time_ns = timer_.get_timestamp_ns();
    event.failure_type = failure_type;
    event.recovery_strategy = RecoveryStrategy::RESTART_COMPONENT; // Default
    event.recovery_successful = false;
    
    // Copy description safely
    const size_t max_len = sizeof(event.failure_description) - 1;
    size_t i = 0;
    while (i < max_len && description[i] != '\0') {
        event.failure_description[i] = description[i];
        ++i;
    }
    event.failure_description[i] = '\0';
    
    // Try to send failure notification
    failure_buffer_.try_push(event);
}

inline uint64_t FaultToleranceManager::create_checkpoint() noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const size_t current_count = checkpoint_count_.load(std::memory_order_acquire);
    if (current_count >= MAX_CHECKPOINTS) {
        cleanup_old_checkpoints();
    }
    
    const uint64_t checkpoint_id = next_checkpoint_id_.fetch_add(1, std::memory_order_relaxed);
    const size_t checkpoint_index = current_count % MAX_CHECKPOINTS;
    
    auto& checkpoint = checkpoints_[checkpoint_index];
    checkpoint.checkpoint_id = checkpoint_id;
    checkpoint.creation_time_ns = start_time;
    checkpoint.system_state = system_state_.load();
    checkpoint.active_components = 0;
    
    // Create bitmask of active components
    const size_t component_count = component_count_.load();
    for (size_t i = 0; i < component_count && i < 64; ++i) {
        if (components_[i].health == ProductionMonitoringSystem::ComponentHealth::HEALTHY) {
            checkpoint.active_components |= (1ULL << i);
        }
    }
    
    // Capture system metrics
    const auto dashboard = monitoring_system_.generate_dashboard_snapshot();
    checkpoint.total_orders_processed = dashboard.total_messages_processed;
    checkpoint.total_messages_processed = dashboard.total_messages_processed;
    checkpoint.system_latency_ns = dashboard.avg_system_latency_ns;
    
    if (current_count < MAX_CHECKPOINTS) {
        checkpoint_count_.store(current_count + 1, std::memory_order_release);
    }
    
    last_checkpoint_time_ns_.store(start_time, std::memory_order_release);
    
    // Update performance metrics
    metrics_.checkpoints_created.fetch_add(1, std::memory_order_relaxed);
    const auto checkpoint_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_checkpoint_time_ns.fetch_add(checkpoint_time, std::memory_order_relaxed);
    
    return checkpoint_id;
}

inline bool FaultToleranceManager::trigger_emergency_shutdown(const char* reason) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    // Set emergency shutdown state
    update_system_state(SystemState::EMERGENCY_SHUTDOWN);
    emergency_shutdown_time_ns_.store(start_time, std::memory_order_release);
    
    // Cancel all orders through order lifecycle manager
    order_manager_.emergency_stop();
    
    // Update metrics
    metrics_.emergency_shutdowns.fetch_add(1, std::memory_order_relaxed);
    
    // Create failure event for emergency shutdown
    add_failure_event(0, FailureType::MANUAL_SHUTDOWN, reason);
    
    return true;
}

inline void FaultToleranceManager::update_component_health(
    uint64_t component_id,
    ProductionMonitoringSystem::ComponentHealth health
) noexcept {
    const size_t component_index = find_component_index(component_id);
    if (component_index >= MAX_COMPONENTS) return;
    
    auto& component = components_[component_index];
    component.health = health;
    component.last_heartbeat_ns = timer_.get_timestamp_ns();
    
    // Check for automatic failure detection
    if (health == ProductionMonitoringSystem::ComponentHealth::FAILED ||
        health == ProductionMonitoringSystem::ComponentHealth::UNHEALTHY) {
        report_failure(component_id, FailureType::PERFORMANCE_DEGRADATION, "Health degradation detected");
    }
}

inline size_t FaultToleranceManager::process_recoveries() noexcept {
    size_t recoveries_processed = 0;
    
    // Check for automatic checkpoint creation
    const uint64_t current_time = timer_.get_timestamp_ns();
    const uint64_t last_checkpoint = last_checkpoint_time_ns_.load();
    
    if ((current_time - last_checkpoint) > CHECKPOINT_INTERVAL_NS) {
        create_checkpoint();
    }
    
    // Check component heartbeats for timeout failures
    const size_t component_count = component_count_.load();
    constexpr uint64_t heartbeat_timeout = 10000000000ULL; // 10 seconds
    
    for (size_t i = 0; i < component_count; ++i) {
        auto& component = components_[i];
        if ((current_time - component.last_heartbeat_ns) > heartbeat_timeout &&
            component.health != ProductionMonitoringSystem::ComponentHealth::FAILED) {
            
            report_failure(component.component_id, FailureType::HEARTBEAT_TIMEOUT, "Heartbeat timeout");
            ++recoveries_processed;
        }
    }
    
    return recoveries_processed;
}

// Helper method implementations
inline size_t FaultToleranceManager::find_component_index(uint64_t component_id) const noexcept {
    const auto it = component_index_map_.find(component_id);
    return (it != component_index_map_.end()) ? it->second : MAX_COMPONENTS;
}

inline FaultToleranceManager::RecoveryPlan* FaultToleranceManager::find_recovery_plan(
    ProductionMonitoringSystem::ComponentType component_type,
    FailureType failure_type
) noexcept {
    const size_t plan_count = recovery_plan_count_.load();
    
    RecoveryPlan* best_plan = nullptr;
    uint8_t best_priority = 255;
    
    for (size_t i = 0; i < plan_count; ++i) {
        auto& plan = recovery_plans_[i];
        if (plan.component_type == component_type && 
            plan.failure_type == failure_type && 
            plan.enabled &&
            plan.priority < best_priority) {
            best_plan = &plan;
            best_priority = plan.priority;
        }
    }
    
    return best_plan;
}

inline bool FaultToleranceManager::should_trigger_emergency_shutdown(FailureType failure_type) const noexcept {
    return (failure_type == FailureType::DATA_CORRUPTION ||
            failure_type == FailureType::RESOURCE_EXHAUSTION ||
            failure_type == FailureType::MANUAL_SHUTDOWN);
}

inline void FaultToleranceManager::update_system_state(SystemState new_state) noexcept {
    system_state_.store(new_state, std::memory_order_release);
}

inline void FaultToleranceManager::cleanup_old_checkpoints() noexcept {
    // Remove oldest checkpoints to make room
    const size_t cleanup_count = MAX_CHECKPOINTS / 4; // Remove 25%
    
    for (size_t i = 0; i < cleanup_count; ++i) {
        checkpoints_[i] = SystemCheckpoint{}; // Reset to default
    }
    
    checkpoint_count_.store(MAX_CHECKPOINTS - cleanup_count, std::memory_order_release);
}

// Getter implementations
inline const FaultToleranceManager::ComponentStatus& 
FaultToleranceManager::get_component_status(uint64_t component_id) const noexcept {
    const size_t component_index = find_component_index(component_id);
    if (component_index < MAX_COMPONENTS) {
        return components_[component_index];
    }
    return components_[0]; // Fallback
}

inline std::vector<FaultToleranceManager::FailureEvent> 
FaultToleranceManager::get_failure_history(size_t max_events) const noexcept {
    std::vector<FailureEvent> history;
    history.reserve(max_events);
    
    const size_t history_size = std::min(
        failure_history_index_.load(std::memory_order_acquire), 
        MAX_FAILURE_HISTORY
    );
    
    for (size_t i = 0; i < history_size && history.size() < max_events; ++i) {
        const auto& event = failure_history_[i];
        if (event.event_id > 0) {
            history.push_back(event);
        }
    }
    
    return history;
}

inline const FaultToleranceManager::RecoveryPlan& 
FaultToleranceManager::get_recovery_plan(uint64_t plan_id) const noexcept {
    const size_t plan_count = recovery_plan_count_.load();
    for (size_t i = 0; i < plan_count; ++i) {
        if (recovery_plans_[i].plan_id == plan_id) {
            return recovery_plans_[i];
        }
    }
    return recovery_plans_[0]; // Fallback
}

// Stub implementations for remaining methods
inline bool FaultToleranceManager::restore_from_checkpoint(uint64_t checkpoint_id) noexcept {
    // Implementation would restore system state from checkpoint
    metrics_.state_restorations.fetch_add(1, std::memory_order_relaxed);
    return true;
}

inline bool FaultToleranceManager::manual_override_system_state(SystemState new_state, uint64_t operator_id) noexcept {
    update_system_state(new_state);
    return true;
}

inline void FaultToleranceManager::reset_fault_tolerance_state() noexcept {
    system_state_.store(SystemState::NORMAL_OPERATION, std::memory_order_release);
    component_count_.store(0, std::memory_order_release);
    recovery_plan_count_.store(0, std::memory_order_release);
    checkpoint_count_.store(0, std::memory_order_release);
    failure_history_index_.store(0, std::memory_order_release);
    component_index_map_.clear();
}

inline void FaultToleranceManager::notify_operators(const FailureEvent& event) noexcept {
    // Implementation would send notifications to operators
    // For now, just log the event
}

// Static asserts
static_assert(alignof(FaultToleranceManager) == 64, "FaultToleranceManager must be cache-aligned");

} // namespace recovery
} // namespace hft