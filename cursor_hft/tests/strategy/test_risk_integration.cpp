#include <gtest/gtest.h>
#include <vector>
#include <chrono>

#include "hft/strategy/risk_integration.hpp"

using namespace hft::strategy;
using namespace hft::market_data;

class RiskIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        risk_framework_ = std::make_unique<RiskIntegrationFramework>();
    }

    void TearDown() override {
        risk_framework_.reset();
    }

    RiskCheckRequest create_risk_request(TreasuryType instrument, int64_t quantity, 
                                       double price, bool enhanced = false) {
        return RiskCheckRequest(instrument, quantity, Price32nd::from_decimal(price), enhanced);
    }

    std::unique_ptr<RiskIntegrationFramework> risk_framework_;
};

// Test Layer 1 (fast) risk checks
TEST_F(RiskIntegrationTest, Layer1BasicChecks) {
    // Normal trade should pass
    auto normal_request = create_risk_request(TreasuryType::Note_10Y, 10000000, 102.5); // $10M
    auto result = risk_framework_->perform_layer1_check(normal_request);
    EXPECT_EQ(result, RiskCheckResult::APPROVED);
    
    // Position limit violation
    auto large_position_request = create_risk_request(TreasuryType::Note_10Y, 150000000, 102.5); // $150M
    result = risk_framework_->perform_layer1_check(large_position_request);
    EXPECT_EQ(result, RiskCheckResult::TRADE_REJECTED);
    
    // Test emergency halt scenario
    risk_framework_->activate_emergency_halt();
    result = risk_framework_->perform_layer1_check(normal_request);
    EXPECT_EQ(result, RiskCheckResult::EMERGENCY_HALT);
    
    risk_framework_->deactivate_emergency_halt();
    result = risk_framework_->perform_layer1_check(normal_request);
    EXPECT_EQ(result, RiskCheckResult::APPROVED);
}

// Test Layer 1 performance requirements
TEST_F(RiskIntegrationTest, Layer1Performance) {
    constexpr int num_iterations = 100000;
    std::vector<uint64_t> latencies;
    latencies.reserve(num_iterations);
    
    auto test_request = create_risk_request(TreasuryType::Note_10Y, 10000000, 102.5);
    
    for (int i = 0; i < num_iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = risk_framework_->perform_layer1_check(test_request);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(latency);
        
        EXPECT_EQ(result, RiskCheckResult::APPROVED);
    }
    
    std::sort(latencies.begin(), latencies.end());
    auto median = latencies[num_iterations / 2];
    auto p95 = latencies[static_cast<size_t>(num_iterations * 0.95)];
    auto p99 = latencies[static_cast<size_t>(num_iterations * 0.99)];
    
    // Performance assertions for Layer 1 (50ns target)
    EXPECT_LE(median, 50);
    EXPECT_LE(p95, 100);
    EXPECT_LE(p99, 200);
    
    std::cout << "Layer 1 Risk Check Latency Statistics:\n";
    std::cout << "  Median: " << median << "ns\n";
    std::cout << "  95th percentile: " << p95 << "ns\n";
    std::cout << "  99th percentile: " << p99 << "ns\n";
}

// Test Layer 2 (enhanced) risk checks
TEST_F(RiskIntegrationTest, Layer2EnhancedChecks) {
    // Set up some portfolio risk state
    risk_framework_->update_layer2_state(
        30000.0,    // portfolio_dv01
        0.4,        // concentration_ratio
        0.6,        // correlation_exposure
        1500000.0,  // value_at_risk
        3000000.0   // stress_test_loss
    );
    
    // Normal enhanced check should pass
    auto normal_request = create_risk_request(TreasuryType::Note_10Y, 5000000, 102.5, true);
    auto result = risk_framework_->perform_layer2_check(normal_request);
    EXPECT_EQ(result, RiskCheckResult::APPROVED);
    
    // Test DV01 limit violation
    auto high_dv01_request = create_risk_request(TreasuryType::Bond_30Y, 15000000, 103.0, true); // High DV01
    result = risk_framework_->perform_layer2_check(high_dv01_request);
    // This might trigger position reduction depending on current DV01
    EXPECT_TRUE(result == RiskCheckResult::APPROVED || result == RiskCheckResult::POSITION_REDUCE);
    
    // Test with high risk metrics that should trigger warnings/restrictions
    risk_framework_->update_layer2_state(
        55000.0,    // portfolio_dv01 (above limit)
        0.7,        // concentration_ratio (above limit)
        0.9,        // correlation_exposure (above limit)
        2500000.0,  // value_at_risk (above limit)
        6000000.0   // stress_test_loss (above limit)
    );
    
    result = risk_framework_->perform_layer2_check(normal_request);
    EXPECT_TRUE(result == RiskCheckResult::WARNING_ISSUED || 
                result == RiskCheckResult::POSITION_REDUCE ||
                result == RiskCheckResult::EMERGENCY_HALT);
}

// Test Layer 2 performance requirements
TEST_F(RiskIntegrationTest, Layer2Performance) {
    constexpr int num_iterations = 50000;
    std::vector<uint64_t> latencies;
    latencies.reserve(num_iterations);
    
    auto test_request = create_risk_request(TreasuryType::Note_10Y, 10000000, 102.5, true);
    
    for (int i = 0; i < num_iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = risk_framework_->perform_layer2_check(test_request);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(latency);
    }
    
    std::sort(latencies.begin(), latencies.end());
    auto median = latencies[num_iterations / 2];
    auto p95 = latencies[static_cast<size_t>(num_iterations * 0.95)];
    
    // Performance assertions for Layer 2 (400ns target)
    EXPECT_LE(median, 400);
    EXPECT_LE(p95, 600);
    
    std::cout << "Layer 2 Risk Check Latency Statistics:\n";
    std::cout << "  Median: " << median << "ns\n";
    std::cout << "  95th percentile: " << p95 << "ns\n";
}

// Test comprehensive risk checks (Layer 1 + Layer 2)
TEST_F(RiskIntegrationTest, ComprehensiveRiskChecks) {
    // Test normal comprehensive check
    auto normal_request = create_risk_request(TreasuryType::Note_10Y, 10000000, 102.5, true);
    auto response = risk_framework_->perform_comprehensive_check(normal_request);
    
    EXPECT_EQ(response.result, RiskCheckResult::APPROVED);
    EXPECT_EQ(response.max_severity, RiskViolationSeverity::NONE);
    EXPECT_GT(response.check_latency_ns, 0);
    EXPECT_LE(response.check_latency_ns, 450); // Combined target: 50ns + 400ns
    
    // Test Layer 1 failure (should not proceed to Layer 2)
    auto large_request = create_risk_request(TreasuryType::Note_10Y, 150000000, 102.5, true);
    response = risk_framework_->perform_comprehensive_check(large_request);
    
    EXPECT_EQ(response.result, RiskCheckResult::TRADE_REJECTED);
    EXPECT_EQ(response.max_severity, RiskViolationSeverity::CRITICAL);
    EXPECT_LE(response.check_latency_ns, 100); // Should be fast Layer 1 only
    
    // Test enhanced checks only (without Layer 2)
    auto layer1_only_request = create_risk_request(TreasuryType::Note_10Y, 10000000, 102.5, false);
    response = risk_framework_->perform_comprehensive_check(layer1_only_request);
    
    EXPECT_EQ(response.result, RiskCheckResult::APPROVED);
    EXPECT_LE(response.check_latency_ns, 50); // Should be Layer 1 only
}

// Test position and state updates
TEST_F(RiskIntegrationTest, StateUpdates) {
    // Update Layer 1 state
    risk_framework_->update_layer1_state(TreasuryType::Note_10Y, 10000000, 50000);
    
    auto layer1_state = risk_framework_->get_layer1_state();
    EXPECT_EQ(layer1_state.current_positions[static_cast<size_t>(TreasuryType::Note_10Y)].load(), 10000000);
    EXPECT_EQ(layer1_state.daily_realized_pnl.load(), 50000);
    EXPECT_EQ(layer1_state.orders_today.load(), 1); // Should increment order count
    
    // Multiple updates should accumulate
    risk_framework_->update_layer1_state(TreasuryType::Note_10Y, 5000000, -20000);
    
    layer1_state = risk_framework_->get_layer1_state();
    EXPECT_EQ(layer1_state.current_positions[static_cast<size_t>(TreasuryType::Note_10Y)].load(), 15000000);
    EXPECT_EQ(layer1_state.daily_realized_pnl.load(), 30000);
    EXPECT_EQ(layer1_state.orders_today.load(), 2);
    
    // Update Layer 2 state
    risk_framework_->update_layer2_state(25000.0, 0.5, 0.7, 1800000.0, 4000000.0);
    
    auto layer2_state = risk_framework_->get_layer2_state();
    EXPECT_DOUBLE_EQ(layer2_state.portfolio_dv01.load(), 25000.0);
    EXPECT_DOUBLE_EQ(layer2_state.concentration_ratio.load(), 0.5);
    EXPECT_DOUBLE_EQ(layer2_state.correlation_exposure.load(), 0.7);
    EXPECT_DOUBLE_EQ(layer2_state.value_at_risk.load(), 1800000.0);
    EXPECT_DOUBLE_EQ(layer2_state.stress_test_loss.load(), 4000000.0);
}

// Test emergency halt functionality
TEST_F(RiskIntegrationTest, EmergencyHaltFunctionality) {
    EXPECT_FALSE(risk_framework_->is_emergency_halt_active());
    
    // Activate emergency halt
    risk_framework_->activate_emergency_halt();
    EXPECT_TRUE(risk_framework_->is_emergency_halt_active());
    
    // All Layer 1 checks should fail during emergency halt
    auto test_request = create_risk_request(TreasuryType::Note_10Y, 1000000, 102.5);
    auto result = risk_framework_->perform_layer1_check(test_request);
    EXPECT_EQ(result, RiskCheckResult::EMERGENCY_HALT);
    
    // Comprehensive checks should also fail
    auto response = risk_framework_->perform_comprehensive_check(test_request);
    EXPECT_EQ(response.result, RiskCheckResult::EMERGENCY_HALT);
    EXPECT_EQ(response.max_severity, RiskViolationSeverity::EMERGENCY);
    
    // Deactivate emergency halt
    risk_framework_->deactivate_emergency_halt();
    EXPECT_FALSE(risk_framework_->is_emergency_halt_active());
    
    // Checks should now pass
    result = risk_framework_->perform_layer1_check(test_request);
    EXPECT_EQ(result, RiskCheckResult::APPROVED);
}

// Test violation detection and reporting
TEST_F(RiskIntegrationTest, ViolationDetection) {
    // Set up state that will trigger violations
    risk_framework_->update_layer1_state(TreasuryType::Note_10Y, 80000000, -800000); // Large position, loss
    risk_framework_->update_layer2_state(55000.0, 0.8, 0.9, 2500000.0, 6000000.0); // Over limits
    
    auto violation_request = create_risk_request(TreasuryType::Note_10Y, 25000000, 102.5, true);
    auto response = risk_framework_->perform_comprehensive_check(violation_request);
    
    // Should detect violations
    EXPECT_NE(response.result, RiskCheckResult::APPROVED);
    EXPECT_GT(response.max_severity, RiskViolationSeverity::NONE);
    EXPECT_GT(response.violations_bitmask, 0);
    
    if (response.max_severity >= RiskViolationSeverity::CRITICAL) {
        EXPECT_GT(response.violation_event.event_id, 0);
        EXPECT_EQ(response.violation_event.instrument, TreasuryType::Note_10Y);
        EXPECT_GT(response.violation_event.timestamp, 0);
    }
}

// Test multi-instrument risk tracking
TEST_F(RiskIntegrationTest, MultiInstrumentRiskTracking) {
    // Add positions across multiple instruments
    std::array<TreasuryType, 6> instruments = {
        TreasuryType::Bill_3M, TreasuryType::Bill_6M, TreasuryType::Note_2Y,
        TreasuryType::Note_5Y, TreasuryType::Note_10Y, TreasuryType::Bond_30Y
    };
    
    std::array<int64_t, 6> quantities = {5000000, 8000000, 12000000, 15000000, 20000000, 10000000};
    
    for (size_t i = 0; i < 6; ++i) {
        risk_framework_->update_layer1_state(instruments[i], quantities[i], 0);
    }
    
    auto layer1_state = risk_framework_->get_layer1_state();
    
    // Verify all positions were recorded
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(layer1_state.current_positions[i].load(), quantities[i]);
    }
    
    // Test risk checks for each instrument
    for (size_t i = 0; i < 6; ++i) {
        auto request = create_risk_request(instruments[i], 5000000, 102.0);
        auto result = risk_framework_->perform_layer1_check(request);
        // All should pass since we're within individual position limits
        EXPECT_EQ(result, RiskCheckResult::APPROVED);
    }
}

// Test performance statistics and monitoring
TEST_F(RiskIntegrationTest, PerformanceStatistics) {
    constexpr int num_layer1_checks = 10000;
    constexpr int num_layer2_checks = 5000;
    constexpr int num_comprehensive_checks = 2000;
    
    // Perform various types of checks
    for (int i = 0; i < num_layer1_checks; ++i) {
        auto request = create_risk_request(TreasuryType::Note_10Y, 1000000 + i, 102.5);
        risk_framework_->perform_layer1_check(request);
    }
    
    for (int i = 0; i < num_layer2_checks; ++i) {
        auto request = create_risk_request(TreasuryType::Note_5Y, 1000000 + i, 101.5, true);
        risk_framework_->perform_layer2_check(request);
    }
    
    for (int i = 0; i < num_comprehensive_checks; ++i) {
        auto request = create_risk_request(TreasuryType::Note_2Y, 1000000 + i, 100.5, true);
        risk_framework_->perform_comprehensive_check(request);
    }
    
    auto stats = risk_framework_->get_performance_stats();
    
    EXPECT_EQ(stats.layer1_checks_processed, num_layer1_checks + num_comprehensive_checks);
    EXPECT_EQ(stats.layer2_checks_processed, num_layer2_checks + num_comprehensive_checks);
    EXPECT_GT(stats.average_layer1_latency_ns, 0);
    EXPECT_GT(stats.average_layer2_latency_ns, 0);
    EXPECT_LE(stats.average_layer1_latency_ns, 50);  // Layer 1 target
    EXPECT_LE(stats.average_layer2_latency_ns, 400); // Layer 2 target
    EXPECT_FALSE(stats.emergency_halt_active);
    
    std::cout << "Risk Framework Performance Statistics:\n";
    std::cout << "  Layer 1 checks: " << stats.layer1_checks_processed << "\n";
    std::cout << "  Layer 2 checks: " << stats.layer2_checks_processed << "\n";
    std::cout << "  Average Layer 1 latency: " << stats.average_layer1_latency_ns << "ns\n";
    std::cout << "  Average Layer 2 latency: " << stats.average_layer2_latency_ns << "ns\n";
    std::cout << "  Violation rate: " << (stats.violation_rate * 100.0) << "%\n";
}

// Test stress scenarios and edge cases
TEST_F(RiskIntegrationTest, StressScenarios) {
    // Test rapid fire risk checks (high frequency scenario)
    constexpr int rapid_fire_count = 50000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < rapid_fire_count; ++i) {
        auto request = create_risk_request(
            static_cast<TreasuryType>(i % 6), 
            1000000 + (i % 1000000), 
            102.0 + (i % 100) * 0.01
        );
        
        if (i % 10 == 0) {
            // Include some comprehensive checks
            request.use_enhanced_checks = true;
            risk_framework_->perform_comprehensive_check(request);
        } else {
            risk_framework_->perform_layer1_check(request);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    auto avg_time_per_check = total_time / static_cast<double>(rapid_fire_count);
    
    std::cout << "Rapid fire test: " << rapid_fire_count << " checks in " << total_time << "μs\n";
    std::cout << "Average time per check: " << avg_time_per_check << "μs\n";
    
    // Should maintain performance under stress
    EXPECT_LT(avg_time_per_check, 1.0); // Average under 1μs per check
    
    // Test extreme values
    auto extreme_request = create_risk_request(TreasuryType::Bond_30Y, INT64_MAX, 999999.0);
    auto result = risk_framework_->perform_layer1_check(extreme_request);
    EXPECT_EQ(result, RiskCheckResult::TRADE_REJECTED); // Should handle gracefully
    
    // Test zero/negative quantities
    auto zero_request = create_risk_request(TreasuryType::Note_10Y, 0, 102.5);
    result = risk_framework_->perform_layer1_check(zero_request);
    EXPECT_EQ(result, RiskCheckResult::APPROVED); // Zero quantity should be allowed
    
    auto negative_request = create_risk_request(TreasuryType::Note_10Y, -50000000, 102.5);
    result = risk_framework_->perform_layer1_check(negative_request);
    // Should handle negative quantities (sell orders)
    EXPECT_TRUE(result == RiskCheckResult::APPROVED || result == RiskCheckResult::TRADE_REJECTED);
}