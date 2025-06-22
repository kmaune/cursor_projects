#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <chrono>
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"
#include "hft/market_data/treasury_instruments.hpp"
#include "hft/trading/order_lifecycle_manager.hpp"

namespace hft {
namespace trading {

using namespace hft::market_data;

/**
 * @brief Advanced position reconciliation and settlement management
 * 
 * Provides comprehensive position tracking and settlement processing:
 * - Real-time position tracking across all venues
 * - End-of-day settlement calculations and reporting
 * - Position break detection and reconciliation
 * - Treasury-specific settlement conventions (T+1)
 * - Automated position correction workflows
 * - Performance metrics and audit trails
 * 
 * Performance targets:
 * - Position update: <200ns
 * - Settlement calculation: <1μs
 * - Break detection: <500ns
 * - Reconciliation processing: <750ns
 * - Report generation: <2μs
 */
class alignas(64) PositionReconciliationManager {
public:
    static constexpr size_t MAX_INSTRUMENTS = 6;
    static constexpr size_t MAX_VENUES = 8;
    static constexpr size_t MAX_SETTLEMENT_ENTRIES = 10000;
    static constexpr size_t MAX_POSITION_HISTORY = 100000;
    static constexpr size_t MAX_BREAKS = 1000;
    
    // Settlement status for Treasury instruments
    enum class SettlementStatus : uint8_t {
        PENDING = 0,        // Settlement pending
        CONFIRMED = 1,      // Settlement confirmed
        SETTLED = 2,        // Settlement completed
        FAILED = 3,         // Settlement failed
        CANCELLED = 4       // Settlement cancelled
    };
    
    // Position break types
    enum class BreakType : uint8_t {
        POSITION_MISMATCH = 0,      // Position quantity mismatch
        TRADE_MISMATCH = 1,         // Trade not found
        PRICE_MISMATCH = 2,         // Price discrepancy
        TIMING_MISMATCH = 3,        // Settlement timing issue
        VENUE_DISCONNECT = 4,       // Venue connectivity issue
        DATA_CORRUPTION = 5         // Data integrity issue
    };
    
    // Real-time position tracking per instrument/venue
    struct alignas(64) VenuePosition {
        TreasuryType instrument;                            // Treasury instrument (1 byte)
        OrderLifecycleManager::VenueType venue;            // Trading venue (1 byte)
        uint8_t _pad0[6];                                   // Padding (6 bytes)
        int64_t net_position = 0;                           // Net position quantity (8 bytes)
        int64_t pending_settlement = 0;                     // Pending settlement quantity (8 bytes)
        double weighted_average_cost = 0.0;                 // WACPE for P&L calculation (8 bytes)
        double unrealized_pnl = 0.0;                        // Unrealized P&L (8 bytes)
        double realized_pnl = 0.0;                          // Realized P&L (8 bytes)
        uint64_t last_update_time_ns = 0;                   // Last position update (8 bytes)
        uint64_t last_trade_time_ns = 0;                    // Last trade timestamp (8 bytes)
        // Total: 1+1+6+8+8+8+8+8+8+8 = 64 bytes
        
        VenuePosition() noexcept : _pad0{} {}
    };
    static_assert(sizeof(VenuePosition) == 64, "VenuePosition must be 64 bytes");
    
    // Settlement instruction for end-of-day processing
    struct alignas(64) SettlementInstruction {
        uint64_t settlement_id = 0;                         // Unique settlement ID (8 bytes)
        TreasuryType instrument;                            // Treasury instrument (1 byte)
        OrderLifecycleManager::VenueType venue;            // Settlement venue (1 byte)
        SettlementStatus status = SettlementStatus::PENDING; // Settlement status (1 byte)
        uint8_t _pad0[5];                                   // Padding (5 bytes)
        int64_t net_quantity = 0;                           // Net settlement quantity (8 bytes)
        Price32nd settlement_price;                         // Settlement price (8 bytes)
        double settlement_value = 0.0;                      // Settlement value (8 bytes)
        uint64_t trade_date_ns = 0;                         // Trade date (8 bytes)
        uint64_t settlement_date_ns = 0;                    // Settlement date (T+1) (8 bytes)
        uint64_t created_time_ns = 0;                       // Instruction creation time (8 bytes)
        // Total: 8+1+1+1+5+8+8+8+8+8+8 = 64 bytes
        
        SettlementInstruction() noexcept : _pad0{} {}
    };
    static_assert(sizeof(SettlementInstruction) == 64, "SettlementInstruction must be 64 bytes");
    
    // Position break for reconciliation
    struct alignas(64) PositionBreak {
        uint64_t break_id = 0;                              // Unique break ID (8 bytes)
        uint64_t detection_time_ns = 0;                     // Break detection time (8 bytes)
        TreasuryType instrument;                            // Affected instrument (1 byte)
        OrderLifecycleManager::VenueType venue;            // Affected venue (1 byte)
        BreakType break_type;                               // Type of break (1 byte)
        bool resolved = false;                              // Resolution status (1 byte)
        uint8_t _pad0[4];                                   // Padding (4 bytes)
        int64_t expected_position = 0;                      // Expected position (8 bytes)
        int64_t actual_position = 0;                        // Actual venue position (8 bytes)
        int64_t variance = 0;                               // Position variance (8 bytes)
        uint64_t resolution_time_ns = 0;                    // Resolution timestamp (8 bytes)
        char description[16];                               // Break description (16 bytes)
        // Total: 8+8+1+1+1+1+4+8+8+8+8+16 = 72 bytes, pad to 128
        uint8_t _pad1[56];                                  // Padding to 128 bytes
        
        PositionBreak() noexcept : _pad0{}, _pad1{} { description[0] = '\0'; }
    };
    static_assert(sizeof(PositionBreak) == 128, "PositionBreak must be 128 bytes");
    
    // Position history entry for audit trail
    struct alignas(64) PositionHistoryEntry {
        uint64_t entry_id = 0;                              // Unique entry ID (8 bytes)
        uint64_t timestamp_ns = 0;                          // Entry timestamp (8 bytes)
        uint64_t order_id = 0;                              // Related order ID (8 bytes)
        TreasuryType instrument;                            // Treasury instrument (1 byte)
        OrderLifecycleManager::VenueType venue;            // Trading venue (1 byte)
        OrderSide side;                                     // Trade side (1 byte)
        uint8_t _pad0[5];                                   // Padding (5 bytes)
        int64_t position_change = 0;                        // Position change (8 bytes)
        int64_t new_position = 0;                           // New total position (8 bytes)
        Price32nd trade_price;                              // Trade price (8 bytes)
        double trade_value = 0.0;                           // Trade value (8 bytes)
        // Total: 8+8+8+1+1+1+5+8+8+8+8 = 64 bytes
        
        PositionHistoryEntry() noexcept : _pad0{} {}
    };
    static_assert(sizeof(PositionHistoryEntry) == 64, "PositionHistoryEntry must be 64 bytes");
    
    // Daily settlement report
    struct SettlementReport {
        uint64_t report_date_ns = 0;
        std::array<SettlementInstruction, MAX_SETTLEMENT_ENTRIES> settlements;
        size_t settlement_count = 0;
        double total_settlement_value = 0.0;
        size_t pending_settlements = 0;
        size_t failed_settlements = 0;
        uint64_t report_generation_time_ns = 0;
    };
    
    // Performance metrics
    struct PerformanceMetrics {
        std::atomic<uint64_t> position_updates_processed{0};
        std::atomic<uint64_t> settlement_instructions_generated{0};
        std::atomic<uint64_t> breaks_detected{0};
        std::atomic<uint64_t> breaks_resolved{0};
        std::atomic<uint64_t> total_position_update_time_ns{0};
        std::atomic<uint64_t> total_settlement_calc_time_ns{0};
        std::atomic<uint64_t> total_break_detection_time_ns{0};
    };
    
    /**
     * @brief Constructor with infrastructure dependencies
     */
    PositionReconciliationManager(
        OrderLifecycleManager& order_manager,
        hft::ObjectPool<SettlementInstruction, 1024>& settlement_pool,
        hft::SPSCRingBuffer<PositionBreak, 1024>& break_buffer
    ) noexcept;
    
    // No copy or move semantics
    PositionReconciliationManager(const PositionReconciliationManager&) = delete;
    PositionReconciliationManager& operator=(const PositionReconciliationManager&) = delete;
    
    /**
     * @brief Update position from order fill
     * @param instrument Treasury instrument
     * @param venue Trading venue
     * @param side Order side
     * @param quantity Fill quantity
     * @param price Fill price
     * @param order_id Related order ID
     * @return true if position updated successfully
     */
    bool update_position(
        TreasuryType instrument,
        OrderLifecycleManager::VenueType venue,
        OrderSide side,
        uint64_t quantity,
        Price32nd price,
        uint64_t order_id
    ) noexcept;
    
    /**
     * @brief Update market price for unrealized P&L calculation
     * @param instrument Treasury instrument
     * @param market_price Current market price
     */
    void update_market_price(
        TreasuryType instrument,
        Price32nd market_price
    ) noexcept;
    
    /**
     * @brief Reconcile positions with venue-reported positions
     * @param instrument Treasury instrument
     * @param venue Trading venue
     * @param venue_reported_position Position reported by venue
     * @return true if positions match, false if break detected
     */
    bool reconcile_venue_position(
        TreasuryType instrument,
        OrderLifecycleManager::VenueType venue,
        int64_t venue_reported_position
    ) noexcept;
    
    /**
     * @brief Generate end-of-day settlement instructions
     * @param settlement_date Settlement date (T+1)
     * @return Number of settlement instructions generated
     */
    [[nodiscard]] size_t generate_settlement_instructions(
        uint64_t settlement_date
    ) noexcept;
    
    /**
     * @brief Generate daily settlement report
     * @param report_date Date for settlement report
     * @return Settlement report
     */
    [[nodiscard]] SettlementReport generate_settlement_report(
        uint64_t report_date
    ) const noexcept;
    
    /**
     * @brief Get current position for instrument/venue
     * @param instrument Treasury instrument
     * @param venue Trading venue
     * @return Venue position information
     */
    [[nodiscard]] const VenuePosition& get_venue_position(
        TreasuryType instrument,
        OrderLifecycleManager::VenueType venue
    ) const noexcept;
    
    /**
     * @brief Get net position across all venues for instrument
     * @param instrument Treasury instrument
     * @return Net position across all venues
     */
    [[nodiscard]] int64_t get_net_position(TreasuryType instrument) const noexcept;
    
    /**
     * @brief Get total P&L for instrument
     * @param instrument Treasury instrument
     * @return Total realized + unrealized P&L
     */
    [[nodiscard]] double get_total_pnl(TreasuryType instrument) const noexcept;
    
    /**
     * @brief Get unresolved position breaks
     * @param max_breaks Maximum number of breaks to return
     * @return Vector of unresolved breaks
     */
    [[nodiscard]] std::vector<PositionBreak> get_unresolved_breaks(
        size_t max_breaks = 100
    ) const noexcept;
    
    /**
     * @brief Mark position break as resolved
     * @param break_id Break ID to resolve
     * @param resolution_notes Resolution description
     * @return true if break resolved successfully
     */
    bool resolve_position_break(
        uint64_t break_id,
        const char* resolution_notes
    ) noexcept;
    
    /**
     * @brief Get performance metrics
     * @return Performance metrics
     */
    [[nodiscard]] const PerformanceMetrics& get_metrics() const noexcept { return metrics_; }
    
    /**
     * @brief Reset all positions (end-of-day cleanup)
     */
    void reset_daily_positions() noexcept;

private:
    // Infrastructure references
    alignas(64) OrderLifecycleManager& order_manager_;
    alignas(64) hft::ObjectPool<SettlementInstruction, 1024>& settlement_pool_;
    alignas(64) hft::SPSCRingBuffer<PositionBreak, 1024>& break_buffer_;
    alignas(64) hft::HFTTimer timer_;
    
    // Position tracking storage
    alignas(64) std::array<std::array<VenuePosition, MAX_VENUES>, MAX_INSTRUMENTS> positions_;
    alignas(64) std::array<Price32nd, MAX_INSTRUMENTS> current_market_prices_;
    
    // Settlement tracking
    alignas(64) std::array<SettlementInstruction, MAX_SETTLEMENT_ENTRIES> settlement_instructions_;
    alignas(64) std::atomic<size_t> settlement_count_;
    
    // Break tracking
    alignas(64) std::array<PositionBreak, MAX_BREAKS> position_breaks_;
    alignas(64) std::atomic<size_t> break_count_;
    alignas(64) std::atomic<uint64_t> next_break_id_;
    
    // Position history for audit
    alignas(64) std::array<PositionHistoryEntry, MAX_POSITION_HISTORY> position_history_;
    alignas(64) std::atomic<size_t> history_index_;
    
    // Performance tracking
    alignas(64) PerformanceMetrics metrics_;
    
    // Helper methods
    void add_position_history_entry(
        uint64_t order_id,
        TreasuryType instrument,
        OrderLifecycleManager::VenueType venue,
        OrderSide side,
        int64_t position_change,
        int64_t new_position,
        Price32nd price
    ) noexcept;
    
    void detect_position_break(
        TreasuryType instrument,
        OrderLifecycleManager::VenueType venue,
        int64_t expected_position,
        int64_t actual_position,
        BreakType break_type,
        const char* description
    ) noexcept;
    
    void calculate_unrealized_pnl(
        TreasuryType instrument,
        OrderLifecycleManager::VenueType venue
    ) noexcept;
    
    [[nodiscard]] size_t get_position_index(
        TreasuryType instrument,
        OrderLifecycleManager::VenueType venue
    ) const noexcept;
    
    [[nodiscard]] uint64_t get_settlement_date(uint64_t trade_date) const noexcept;
};

// Implementation

inline PositionReconciliationManager::PositionReconciliationManager(
    OrderLifecycleManager& order_manager,
    hft::ObjectPool<SettlementInstruction, 1024>& settlement_pool,
    hft::SPSCRingBuffer<PositionBreak, 1024>& break_buffer
) noexcept
    : order_manager_(order_manager),
      settlement_pool_(settlement_pool),
      break_buffer_(break_buffer),
      timer_(),
      positions_{},
      current_market_prices_{},
      settlement_instructions_{},
      settlement_count_(0),
      position_breaks_{},
      break_count_(0),
      next_break_id_(1),
      position_history_{},
      history_index_(0),
      metrics_{} {
    
    // Initialize positions
    for (size_t inst = 0; inst < MAX_INSTRUMENTS; ++inst) {
        for (size_t venue = 0; venue < MAX_VENUES; ++venue) {
            auto& pos = positions_[inst][venue];
            pos.instrument = static_cast<TreasuryType>(inst);
            pos.venue = static_cast<OrderLifecycleManager::VenueType>(venue);
        }
        
        // Initialize market prices
        current_market_prices_[inst] = Price32nd::from_decimal(100.0); // Default price
    }
}

inline bool PositionReconciliationManager::update_position(
    TreasuryType instrument,
    OrderLifecycleManager::VenueType venue,
    OrderSide side,
    uint64_t quantity,
    Price32nd price,
    uint64_t order_id
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const auto inst_index = static_cast<size_t>(instrument);
    const auto venue_index = static_cast<size_t>(venue);
    
    if (inst_index >= MAX_INSTRUMENTS || venue_index >= MAX_VENUES) {
        return false;
    }
    
    auto& position = positions_[inst_index][venue_index];
    
    // Calculate position change
    const int64_t position_change = (side == OrderSide::BID) ? 
                                   static_cast<int64_t>(quantity) : 
                                   -static_cast<int64_t>(quantity);
    
    const int64_t old_position = position.net_position;
    position.net_position += position_change;
    
    // Update weighted average cost (simplified)
    if (position_change != 0) {
        const double trade_value = static_cast<double>(std::abs(position_change)) * price.to_decimal();
        const double total_value = position.weighted_average_cost * std::abs(old_position) + 
                                  ((position_change > 0) ? trade_value : -trade_value);
        
        if (position.net_position != 0) {
            position.weighted_average_cost = total_value / std::abs(position.net_position);
        }
        
        // Update realized P&L for closing trades
        if ((old_position > 0 && position_change < 0) || (old_position < 0 && position_change > 0)) {
            const int64_t closing_quantity = std::min(std::abs(old_position), std::abs(position_change));
            const double pnl_per_unit = price.to_decimal() - position.weighted_average_cost;
            const double realized_pnl = closing_quantity * pnl_per_unit * ((old_position > 0) ? 1.0 : -1.0);
            position.realized_pnl += realized_pnl;
        }
    }
    
    position.last_update_time_ns = start_time;
    position.last_trade_time_ns = start_time;
    
    // Calculate unrealized P&L
    calculate_unrealized_pnl(instrument, venue);
    
    // Add to position history
    add_position_history_entry(order_id, instrument, venue, side, 
                              position_change, position.net_position, price);
    
    // Update performance metrics
    metrics_.position_updates_processed.fetch_add(1, std::memory_order_relaxed);
    const auto update_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_position_update_time_ns.fetch_add(update_time, std::memory_order_relaxed);
    
    return true;
}

inline void PositionReconciliationManager::update_market_price(
    TreasuryType instrument,
    Price32nd market_price
) noexcept {
    const auto inst_index = static_cast<size_t>(instrument);
    if (inst_index >= MAX_INSTRUMENTS) return;
    
    current_market_prices_[inst_index] = market_price;
    
    // Recalculate unrealized P&L for all venues
    for (size_t venue = 0; venue < MAX_VENUES; ++venue) {
        calculate_unrealized_pnl(instrument, static_cast<OrderLifecycleManager::VenueType>(venue));
    }
}

inline void PositionReconciliationManager::calculate_unrealized_pnl(
    TreasuryType instrument,
    OrderLifecycleManager::VenueType venue
) noexcept {
    const auto inst_index = static_cast<size_t>(instrument);
    const auto venue_index = static_cast<size_t>(venue);
    
    if (inst_index >= MAX_INSTRUMENTS || venue_index >= MAX_VENUES) return;
    
    auto& position = positions_[inst_index][venue_index];
    
    if (position.net_position != 0) {
        const double market_price = current_market_prices_[inst_index].to_decimal();
        const double cost_basis = position.weighted_average_cost;
        const double pnl_per_unit = market_price - cost_basis;
        
        position.unrealized_pnl = position.net_position * pnl_per_unit;
    } else {
        position.unrealized_pnl = 0.0;
    }
}

inline bool PositionReconciliationManager::reconcile_venue_position(
    TreasuryType instrument,
    OrderLifecycleManager::VenueType venue,
    int64_t venue_reported_position
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    
    const auto inst_index = static_cast<size_t>(instrument);
    const auto venue_index = static_cast<size_t>(venue);
    
    if (inst_index >= MAX_INSTRUMENTS || venue_index >= MAX_VENUES) {
        return false;
    }
    
    const auto& position = positions_[inst_index][venue_index];
    const int64_t expected_position = position.net_position;
    const int64_t variance = venue_reported_position - expected_position;
    
    // Check for position break (tolerance of ±1 unit for rounding)
    if (std::abs(variance) > 1) {
        detect_position_break(instrument, venue, expected_position, venue_reported_position,
                             BreakType::POSITION_MISMATCH, "Venue position mismatch");
        
        // Update performance metrics
        const auto detection_time = timer_.get_timestamp_ns() - start_time;
        metrics_.total_break_detection_time_ns.fetch_add(detection_time, std::memory_order_relaxed);
        
        return false;
    }
    
    return true;
}

inline void PositionReconciliationManager::detect_position_break(
    TreasuryType instrument,
    OrderLifecycleManager::VenueType venue,
    int64_t expected_position,
    int64_t actual_position,
    BreakType break_type,
    const char* description
) noexcept {
    const size_t break_index = break_count_.fetch_add(1, std::memory_order_relaxed) % MAX_BREAKS;
    
    auto& pos_break = position_breaks_[break_index];
    pos_break.break_id = next_break_id_.fetch_add(1, std::memory_order_relaxed);
    pos_break.detection_time_ns = timer_.get_timestamp_ns();
    pos_break.instrument = instrument;
    pos_break.venue = venue;
    pos_break.break_type = break_type;
    pos_break.resolved = false;
    pos_break.expected_position = expected_position;
    pos_break.actual_position = actual_position;
    pos_break.variance = actual_position - expected_position;
    pos_break.resolution_time_ns = 0;
    
    // Copy description safely
    const size_t max_len = sizeof(pos_break.description) - 1;
    size_t i = 0;
    while (i < max_len && description[i] != '\0') {
        pos_break.description[i] = description[i];
        ++i;
    }
    pos_break.description[i] = '\0';
    
    // Try to send break notification
    break_buffer_.try_push(pos_break);
    
    metrics_.breaks_detected.fetch_add(1, std::memory_order_relaxed);
}

inline void PositionReconciliationManager::add_position_history_entry(
    uint64_t order_id,
    TreasuryType instrument,
    OrderLifecycleManager::VenueType venue,
    OrderSide side,
    int64_t position_change,
    int64_t new_position,
    Price32nd price
) noexcept {
    const size_t index = history_index_.fetch_add(1, std::memory_order_relaxed) % MAX_POSITION_HISTORY;
    
    auto& entry = position_history_[index];
    entry.entry_id = index;
    entry.timestamp_ns = timer_.get_timestamp_ns();
    entry.order_id = order_id;
    entry.instrument = instrument;
    entry.venue = venue;
    entry.side = side;
    entry.position_change = position_change;
    entry.new_position = new_position;
    entry.trade_price = price;
    entry.trade_value = std::abs(position_change) * price.to_decimal();
}

inline size_t PositionReconciliationManager::generate_settlement_instructions(
    uint64_t settlement_date
) noexcept {
    const auto start_time = timer_.get_timestamp_ns();
    size_t instructions_generated = 0;
    
    for (size_t inst = 0; inst < MAX_INSTRUMENTS; ++inst) {
        for (size_t venue = 0; venue < MAX_VENUES; ++venue) {
            const auto& position = positions_[inst][venue];
            
            if (position.net_position != 0) {
                if (settlement_count_.load() < MAX_SETTLEMENT_ENTRIES) {
                    const size_t settlement_index = settlement_count_.fetch_add(1, std::memory_order_relaxed);
                    
                    auto& instruction = settlement_instructions_[settlement_index];
                    instruction.settlement_id = settlement_index + 1;
                    instruction.instrument = position.instrument;
                    instruction.venue = position.venue;
                    instruction.status = SettlementStatus::PENDING;
                    instruction.net_quantity = position.net_position;
                    instruction.settlement_price = current_market_prices_[inst];
                    instruction.settlement_value = position.net_position * instruction.settlement_price.to_decimal();
                    instruction.trade_date_ns = timer_.get_timestamp_ns();
                    instruction.settlement_date_ns = settlement_date;
                    instruction.created_time_ns = timer_.get_timestamp_ns();
                    
                    ++instructions_generated;
                }
            }
        }
    }
    
    // Update performance metrics
    metrics_.settlement_instructions_generated.fetch_add(instructions_generated, std::memory_order_relaxed);
    const auto calc_time = timer_.get_timestamp_ns() - start_time;
    metrics_.total_settlement_calc_time_ns.fetch_add(calc_time, std::memory_order_relaxed);
    
    return instructions_generated;
}

inline PositionReconciliationManager::SettlementReport 
PositionReconciliationManager::generate_settlement_report(uint64_t report_date) const noexcept {
    SettlementReport report;
    report.report_date_ns = report_date;
    report.report_generation_time_ns = timer_.get_timestamp_ns();
    
    const size_t settlement_count = settlement_count_.load();
    report.settlement_count = std::min(settlement_count, MAX_SETTLEMENT_ENTRIES);
    
    // Copy settlement instructions
    for (size_t i = 0; i < report.settlement_count; ++i) {
        report.settlements[i] = settlement_instructions_[i];
        report.total_settlement_value += settlement_instructions_[i].settlement_value;
        
        if (settlement_instructions_[i].status == SettlementStatus::PENDING) {
            ++report.pending_settlements;
        } else if (settlement_instructions_[i].status == SettlementStatus::FAILED) {
            ++report.failed_settlements;
        }
    }
    
    return report;
}

// Getter implementations
inline const PositionReconciliationManager::VenuePosition& 
PositionReconciliationManager::get_venue_position(
    TreasuryType instrument,
    OrderLifecycleManager::VenueType venue
) const noexcept {
    const auto inst_index = static_cast<size_t>(instrument);
    const auto venue_index = static_cast<size_t>(venue);
    
    if (inst_index < MAX_INSTRUMENTS && venue_index < MAX_VENUES) {
        return positions_[inst_index][venue_index];
    }
    
    return positions_[0][0]; // Fallback
}

inline int64_t PositionReconciliationManager::get_net_position(TreasuryType instrument) const noexcept {
    const auto inst_index = static_cast<size_t>(instrument);
    if (inst_index >= MAX_INSTRUMENTS) return 0;
    
    int64_t net_position = 0;
    for (size_t venue = 0; venue < MAX_VENUES; ++venue) {
        net_position += positions_[inst_index][venue].net_position;
    }
    
    return net_position;
}

inline double PositionReconciliationManager::get_total_pnl(TreasuryType instrument) const noexcept {
    const auto inst_index = static_cast<size_t>(instrument);
    if (inst_index >= MAX_INSTRUMENTS) return 0.0;
    
    double total_pnl = 0.0;
    for (size_t venue = 0; venue < MAX_VENUES; ++venue) {
        const auto& position = positions_[inst_index][venue];
        total_pnl += position.realized_pnl + position.unrealized_pnl;
    }
    
    return total_pnl;
}

inline std::vector<PositionReconciliationManager::PositionBreak> 
PositionReconciliationManager::get_unresolved_breaks(size_t max_breaks) const noexcept {
    std::vector<PositionBreak> unresolved_breaks;
    unresolved_breaks.reserve(max_breaks);
    
    const size_t break_count = break_count_.load();
    const size_t total_breaks = std::min(break_count, MAX_BREAKS);
    
    for (size_t i = 0; i < total_breaks && unresolved_breaks.size() < max_breaks; ++i) {
        const auto& pos_break = position_breaks_[i];
        if (!pos_break.resolved) {
            unresolved_breaks.push_back(pos_break);
        }
    }
    
    return unresolved_breaks;
}

inline bool PositionReconciliationManager::resolve_position_break(
    uint64_t break_id,
    const char* resolution_notes
) noexcept {
    const size_t break_count = break_count_.load();
    const size_t total_breaks = std::min(break_count, MAX_BREAKS);
    
    for (size_t i = 0; i < total_breaks; ++i) {
        auto& pos_break = position_breaks_[i];
        if (pos_break.break_id == break_id && !pos_break.resolved) {
            pos_break.resolved = true;
            pos_break.resolution_time_ns = timer_.get_timestamp_ns();
            
            // Copy resolution notes to description
            const size_t max_len = sizeof(pos_break.description) - 1;
            size_t j = 0;
            while (j < max_len && resolution_notes[j] != '\0') {
                pos_break.description[j] = resolution_notes[j];
                ++j;
            }
            pos_break.description[j] = '\0';
            
            metrics_.breaks_resolved.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    
    return false;
}

inline void PositionReconciliationManager::reset_daily_positions() noexcept {
    // Reset all positions for new trading day
    for (size_t inst = 0; inst < MAX_INSTRUMENTS; ++inst) {
        for (size_t venue = 0; venue < MAX_VENUES; ++venue) {
            auto& position = positions_[inst][venue];
            position.net_position = 0;
            position.pending_settlement = 0;
            position.weighted_average_cost = 0.0;
            position.unrealized_pnl = 0.0;
            position.realized_pnl = 0.0;
            position.last_update_time_ns = 0;
            position.last_trade_time_ns = 0;
        }
    }
    
    // Reset settlement instructions
    settlement_count_.store(0, std::memory_order_relaxed);
    
    // Reset break tracking
    break_count_.store(0, std::memory_order_relaxed);
    next_break_id_.store(1, std::memory_order_relaxed);
}

inline uint64_t PositionReconciliationManager::get_settlement_date(uint64_t trade_date) const noexcept {
    // Treasury settlement is T+1
    constexpr uint64_t one_day_ns = 24ULL * 60ULL * 60ULL * 1000000000ULL;
    return trade_date + one_day_ns;
}

// Static asserts
static_assert(alignof(PositionReconciliationManager) == 64, "PositionReconciliationManager must be cache-aligned");

} // namespace trading
} // namespace hft