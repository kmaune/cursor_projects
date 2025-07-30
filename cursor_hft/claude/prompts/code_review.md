# Code Review Prompt Templates

Reusable prompt templates for comprehensive HFT code reviews. These templates provide structured approaches for different types of code reviews with specialized focus areas.

**Recommended Agent**: `@claude/agents/hft-code-reviewer.md`

## Template Categories

### 1. Performance-Focused Code Review

**Use Case**: Latency-critical code with sub-microsecond requirements  
**Customization Parameters**: `[COMPONENT_NAME]`, `[LATENCY_TARGET]`, `[PERFORMANCE_FOCUS]`, `[REVIEW_SCOPE]`

```
Conduct a performance-focused code review of [COMPONENT_NAME] with [LATENCY_TARGET] latency requirement focusing on [PERFORMANCE_FOCUS].

REVIEW SCOPE:
- Component: [COMPONENT_NAME] 
- Latency target: [LATENCY_TARGET] (strict requirement)
- Performance focus: [PERFORMANCE_FOCUS] (algorithm/memory/cache/concurrency)
- Review scope: [REVIEW_SCOPE] (hot paths/full component/integration points)

PERFORMANCE ANALYSIS REQUIREMENTS:
- Algorithmic complexity analysis (Big-O for time and space)
- Memory allocation identification (must be zero in hot paths)
- Cache behavior analysis (ARM64 64-byte cache line optimization)
- Branch prediction analysis (eliminate unpredictable branches)
- Memory access pattern review (sequential vs random access)

CRITICAL PERFORMANCE CHECKS:
- **Hot path analysis**: Identify critical execution paths under latency requirements
- **Memory allocation audit**: Verify zero allocation in performance-critical sections
- **Cache alignment verification**: Ensure 64-byte alignment for ARM64 optimization
- **Branch optimization**: Check for likely/unlikely hints and branch-free algorithms
- **Template instantiation**: Verify compile-time optimization opportunities

ARCHITECTURE REVIEW:
- Integration with HFTTimer for precise latency measurement
- Object pool usage patterns and efficiency
- Ring buffer implementation and lock-free properties
- Treasury data structure integration (Price32nd, TreasuryTick)
- Component interaction overhead analysis

CODE QUALITY STANDARDS:
```cpp
// Performance code review checklist
namespace performance_review {

// ✓ GOOD: Zero allocation, cache-aligned, template optimized
template<typename T, size_t N>
class alignas(64) PerformantContainer {
    static_assert(N > 0 && N <= 65536, "Reasonable size bounds");
    
    alignas(64) std::array<T, N> data_;
    std::atomic<size_t> head_{0};
    
public:
    // ✓ GOOD: Branch-free, likely annotation, sub-100ns operation
    [[likely]] bool try_push(const T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_acquire);
        if ([[likely]] (current_head < N)) {
            data_[current_head] = item;
            head_.store(current_head + 1, std::memory_order_release);
            return true;
        }
        return false;
    }
};

// ✗ BAD: Dynamic allocation, poor cache behavior
class ProblematicContainer {
    std::vector<int> data_;  // ✗ Dynamic allocation
    std::mutex mtx_;         // ✗ Blocking synchronization
    
public:
    void add_item(int item) {  // ✗ No noexcept, no performance guarantee
        std::lock_guard<std::mutex> lock(mtx_);  // ✗ Blocking
        data_.push_back(item);   // ✗ Potential allocation
    }
};

} // namespace performance_review
```

LATENCY VERIFICATION:
- Benchmark all critical operations against [LATENCY_TARGET]
- Statistical analysis of latency distribution (mean, 95th, 99th percentiles)
- Performance regression testing setup
- ARM64 cycle counter integration validation

OPTIMIZATION RECOMMENDATIONS:
- Specific algorithmic improvements with complexity analysis
- Memory layout optimizations for cache efficiency
- Branch optimization opportunities with likely/unlikely annotations
- Template specialization opportunities for compile-time optimization
- ARM64-specific optimizations (NEON, prefetching, memory ordering)

SUCCESS CRITERIA:
- All critical operations meet [LATENCY_TARGET] requirement
- Zero allocations verified in hot paths
- Cache-efficient memory access patterns confirmed
- Integration compliance with existing HFT infrastructure validated

Provide detailed performance analysis with specific optimization recommendations and latency validation methodology.
```

### 2. Risk and Safety Code Review

**Use Case**: Trading safety and risk management validation  
**Customization Parameters**: `[COMPONENT_TYPE]`, `[RISK_DOMAIN]`, `[SAFETY_REQUIREMENTS]`, `[COMPLIANCE_LEVEL]`

```
Conduct a comprehensive risk and safety review of [COMPONENT_TYPE] focusing on [RISK_DOMAIN] with [SAFETY_REQUIREMENTS] compliance.

SAFETY REVIEW SCOPE:
- Component type: [COMPONENT_TYPE] (strategy/risk manager/order router/position tracker)
- Risk domain: [RISK_DOMAIN] (position/market/operational/systemic)
- Safety requirements: [SAFETY_REQUIREMENTS] (fail-safe/real-time monitoring/audit trail)
- Compliance level: [COMPLIANCE_LEVEL] (regulatory/internal/best practice)

CRITICAL SAFETY CHECKS:
- **Position limit enforcement**: Verify real-time limits with fail-safe mechanisms
- **P&L calculations**: Accuracy validation with mark-to-market updates
- **Risk metric calculations**: Duration, yield sensitivity, correlation calculations
- **Error handling**: Graceful degradation without position exposure
- **State consistency**: Atomic updates and transaction safety

RISK MANAGEMENT VALIDATION:
- Position tracking accuracy and real-time updates
- Risk limit enforcement with automatic circuit breakers
- Stop-loss and take-profit mechanism reliability
- Drawdown protection and position scaling algorithms
- Portfolio-level risk aggregation and monitoring

SAFETY ARCHITECTURE PATTERNS:
```cpp
// Risk management code review patterns
namespace risk_safety_review {

// ✓ GOOD: Comprehensive risk validation with fail-safe defaults
class SafePositionManager {
    std::atomic<int64_t> net_position_{0};
    const int64_t MAX_POSITION = 100'000'000;  // $100M limit
    
public:
    enum class ValidationResult { APPROVED, REJECTED_LIMIT, REJECTED_RISK };
    
    ValidationResult validate_order(const Order& order) noexcept {
        const int64_t current_pos = net_position_.load(std::memory_order_acquire);
        const int64_t new_position = current_pos + order.quantity();
        
        // ✓ GOOD: Multiple safety checks with clear failure modes
        if (std::abs(new_position) > MAX_POSITION) {
            return ValidationResult::REJECTED_LIMIT;
        }
        
        if (calculate_portfolio_risk(new_position) > risk_threshold_) {
            return ValidationResult::REJECTED_RISK;
        }
        
        return ValidationResult::APPROVED;
    }
};

// ✗ BAD: Insufficient error handling and race conditions
class UnsafePositionManager {
    int64_t position_ = 0;  // ✗ Non-atomic in multi-threaded context
    
public:
    bool add_position(int64_t qty) {  // ✗ No validation, no error handling
        position_ += qty;  // ✗ Race condition, no limits
        return true;       // ✗ Always succeeds regardless of risk
    }
};

} // namespace risk_safety_review
```

TRADING SAFETY REQUIREMENTS:
- **Order validation**: Size, price, market hours, instrument validity
- **Position limits**: Real-time enforcement with atomic updates
- **Risk calculations**: Duration risk, yield sensitivity, correlation exposure
- **Circuit breakers**: Automatic trading halt on risk threshold breach
- **Audit trail**: Comprehensive logging for regulatory compliance

ERROR HANDLING VALIDATION:
- Exception safety guarantees (strong exception safety minimum)
- Error propagation without position exposure
- Graceful degradation under system stress
- Recovery mechanisms from error states
- Data consistency under all failure scenarios

COMPLIANCE VERIFICATION:
- Regulatory requirement adherence ([COMPLIANCE_LEVEL] standards)
- Audit trail completeness and accuracy
- Risk reporting accuracy and timeliness
- Position reconciliation and validation
- Trade reporting and compliance monitoring

SUCCESS CRITERIA:
- All trading operations have comprehensive risk validation
- Error conditions handled safely without position exposure
- Real-time risk monitoring with automatic enforcement
- Audit trail meets [COMPLIANCE_LEVEL] requirements
- System maintains safety under all stress conditions

Provide comprehensive safety analysis with specific risk mitigation recommendations and compliance validation.
```

### 3. Architecture and Integration Code Review

**Use Case**: Component integration and system architecture validation  
**Customization Parameters**: `[INTEGRATION_SCOPE]`, `[ARCHITECTURE_PATTERN]`, `[DEPENDENCY_TYPE]`, `[INTERFACE_DESIGN]`

```
Review [INTEGRATION_SCOPE] architecture focusing on [ARCHITECTURE_PATTERN] design with [DEPENDENCY_TYPE] dependency management.

ARCHITECTURE REVIEW SCOPE:
- Integration scope: [INTEGRATION_SCOPE] (component/subsystem/system-wide)
- Architecture pattern: [ARCHITECTURE_PATTERN] (object pools/ring buffers/messaging/state management)
- Dependency type: [DEPENDENCY_TYPE] (compile-time/runtime/interface/data)
- Interface design: [INTERFACE_DESIGN] (template/polymorphic/static/functional)

INTEGRATION VALIDATION:
- **Component interfaces**: Clean abstractions with minimal coupling
- **Data flow design**: Efficient data movement between components
- **Memory management**: Consistent object pool and allocation patterns
- **Timing integration**: HFTTimer usage and performance measurement
- **Error propagation**: Consistent error handling across component boundaries

ARCHITECTURE PATTERN COMPLIANCE:
- Object pool integration for zero-allocation design
- Ring buffer usage for lock-free inter-component communication
- Treasury data structure integration (Price32nd, TreasuryTick, yield calculations)
- HFT infrastructure patterns (timing, memory, messaging frameworks)
- Component lifecycle management and resource cleanup

DESIGN QUALITY ASSESSMENT:
```cpp
// Architecture review patterns
namespace architecture_review {

// ✓ GOOD: Clean interface with dependency injection and testability
template<typename OrderBookT, typename RiskManagerT>
class WellDesignedStrategy {
    OrderBookT& order_book_;
    RiskManagerT& risk_manager_;
    ObjectPool<Order>& order_pool_;
    
public:
    // ✓ GOOD: Clear interface, noexcept guarantee, zero allocation
    void process_tick(const TreasuryTick& tick) noexcept {
        auto signal = generate_signal(tick);
        
        if (risk_manager_.validate_signal(signal)) {
            auto* order = order_pool_.acquire();
            *order = create_order(signal);
            order_book_.submit_order(order);
        }
    }
    
private:
    // ✓ GOOD: Template specialization for compile-time optimization
    template<typename TickT>
    Signal generate_signal(const TickT& tick) noexcept;
};

// ✗ BAD: Tight coupling, poor testability, allocation in hot paths
class PoorlyDesignedStrategy {
    // ✗ Hard-coded dependencies, no injection
    ConcreteOrderBook order_book_;
    ConcreteRiskManager risk_manager_;
    
public:
    void process_tick(const TreasuryTick& tick) {  // ✗ No noexcept
        // ✗ Dynamic allocation in hot path
        auto order = std::make_unique<Order>(create_order(tick));
        
        // ✗ No error handling, tight coupling
        order_book_.submit(std::move(order));
    }
};

} // namespace architecture_review
```

INTERFACE DESIGN VALIDATION:
- Template vs polymorphic interface trade-offs
- Compile-time vs runtime dependency resolution
- Interface segregation and single responsibility adherence
- Dependency inversion and testability
- Performance impact of abstraction layers

INTEGRATION PATTERN COMPLIANCE:
- HFT infrastructure integration (timing, memory, messaging)
- Treasury domain integration (pricing, yield, duration calculations)
- Performance measurement integration (HFTTimer, latency tracking)
- Object lifecycle management and resource cleanup
- Component communication patterns and data flow

TESTING AND MAINTAINABILITY:
- Unit test enablement through dependency injection
- Mock object integration for isolated testing
- Integration test support and framework compatibility
- Code organization and module structure
- Documentation and API clarity

SUCCESS CRITERIA:
- Clean component interfaces with minimal coupling
- Consistent integration with HFT infrastructure patterns
- Efficient data flow with zero-allocation guarantees
- Testable design with clear dependency management
- Maintainable architecture with clear separation of concerns

Provide architectural analysis with specific integration recommendations and design pattern compliance validation.
```

### 4. HFT Domain-Specific Code Review

**Use Case**: Treasury trading domain logic and calculation validation  
**Customization Parameters**: `[DOMAIN_COMPONENT]`, `[CALCULATION_TYPE]`, `[PRECISION_REQUIREMENTS]`, `[MARKET_CONVENTIONS]`

```
Review [DOMAIN_COMPONENT] for Treasury domain accuracy focusing on [CALCULATION_TYPE] with [PRECISION_REQUIREMENTS] precision requirements.

DOMAIN REVIEW SCOPE:
- Domain component: [DOMAIN_COMPONENT] (pricing/yield/duration/strategy logic)
- Calculation type: [CALCULATION_TYPE] (yield-to-maturity/duration/price conversion)
- Precision requirements: [PRECISION_REQUIREMENTS] (decimal places/tick accuracy)
- Market conventions: [MARKET_CONVENTIONS] (day count/settlement/quoting)

TREASURY DOMAIN VALIDATION:
- **Price representation**: 32nd pricing accuracy and conversion correctness
- **Yield calculations**: Proper day count conventions and compounding
- **Duration calculations**: Modified duration and DV01 accuracy
- **Settlement logic**: T+1 settlement and business day handling
- **Market conventions**: Treasury-specific trading rules and conventions

CALCULATION ACCURACY CHECKS:
- Numerical precision and rounding behavior
- Edge case handling (negative yields, long-dated bonds)
- Performance vs accuracy trade-offs
- Validation against market standards
- Error bounds and acceptable tolerance

DOMAIN-SPECIFIC PATTERNS:
```cpp
// Treasury domain code review patterns
namespace treasury_domain_review {

// ✓ GOOD: Precise Treasury calculations with proper conventions
class TreasuryPricingEngine {
public:
    // ✓ GOOD: 32nd precision with proper rounding
    Price32nd calculate_price_from_yield(
        double yield, 
        const BondSpecification& bond_spec) noexcept {
        
        // ✓ GOOD: Proper day count convention
        const double accrual_factor = calculate_accrual_factor(
            bond_spec.issue_date, 
            bond_spec.maturity_date, 
            DayCount::ACTUAL_ACTUAL);
        
        // ✓ GOOD: Standard present value calculation
        double pv = 0.0;
        for (const auto& cashflow : bond_spec.cashflows) {
            const double discount_factor = std::pow(
                1.0 + yield / 2.0, 
                -2.0 * cashflow.time_to_payment);
            pv += cashflow.amount * discount_factor;
        }
        
        // ✓ GOOD: Proper 32nd conversion with rounding
        return Price32nd::from_decimal(pv);
    }
    
    // ✓ GOOD: Accurate modified duration calculation
    double calculate_modified_duration(
        double yield, 
        const BondSpecification& bond_spec) noexcept {
        
        const double price = calculate_price_from_yield(yield, bond_spec).to_decimal();
        const double delta_yield = 0.0001;  // 1 basis point
        
        const double price_up = calculate_price_from_yield(
            yield + delta_yield, bond_spec).to_decimal();
        const double price_down = calculate_price_from_yield(
            yield - delta_yield, bond_spec).to_decimal();
        
        return -(price_up - price_down) / (2.0 * delta_yield * price);
    }
};

// ✗ BAD: Imprecise calculations and incorrect conventions
class IncorrectPricingEngine {
public:
    // ✗ BAD: Float precision loss, incorrect day count
    float calculate_price(float yield, int years_to_maturity) {
        // ✗ Wrong: Using 365 day count instead of actual/actual
        float time_factor = years_to_maturity / 365.0f;
        
        // ✗ Wrong: Incorrect compounding frequency
        return 100.0f / std::pow(1.0f + yield, time_factor);
    }
};

} // namespace treasury_domain_review
```

MARKET CONVENTION COMPLIANCE:
- **Day count conventions**: Actual/Actual for Treasury securities
- **Settlement conventions**: T+1 settlement for Treasury cash market
- **Quoting conventions**: Price in 32nds, yield in decimal
- **Business day handling**: Government bond calendar observance
- **Accrued interest**: Proper calculation and settlement handling

NUMERICAL ACCURACY VALIDATION:
- Floating point precision and error accumulation
- Rounding behavior and market convention compliance
- Edge case handling (near-zero yields, long-dated securities)
- Performance impact of high-precision calculations
- Validation against market data and benchmarks

INTEGRATION WITH HFT INFRASTRUCTURE:
- Price32nd integration with arithmetic operations
- TreasuryTick data structure usage and efficiency
- Performance requirements for real-time calculations
- Memory allocation patterns in calculation routines
- Error handling without trading disruption

SUCCESS CRITERIA:
- All Treasury calculations accurate to [PRECISION_REQUIREMENTS]
- Market convention compliance verified
- Performance requirements met for real-time trading
- Integration compliance with HFT data structures
- Numerical stability under all market conditions

Provide domain expertise analysis with calculation validation and market convention compliance verification.
```

### 5. Pre-Production Code Review

**Use Case**: Final validation before production deployment  
**Customization Parameters**: `[DEPLOYMENT_SCOPE]`, `[CRITICALITY_LEVEL]`, `[VALIDATION_REQUIREMENTS]`, `[ROLLBACK_PLAN]`

```
Conduct comprehensive pre-production review of [DEPLOYMENT_SCOPE] with [CRITICALITY_LEVEL] criticality requiring [VALIDATION_REQUIREMENTS] validation.

PRE-PRODUCTION REVIEW SCOPE:
- Deployment scope: [DEPLOYMENT_SCOPE] (component/strategy/infrastructure)
- Criticality level: [CRITICALITY_LEVEL] (mission-critical/high/medium)
- Validation requirements: [VALIDATION_REQUIREMENTS] (performance/safety/integration)
- Rollback plan: [ROLLBACK_PLAN] (immediate/phased/monitored)

PRODUCTION READINESS CHECKLIST:
- **Performance validation**: All latency targets met with statistical confidence
- **Safety verification**: Risk management and error handling thoroughly tested
- **Integration testing**: Full system integration under realistic load
- **Monitoring setup**: Comprehensive logging and alerting in place
- **Rollback preparation**: Verified rollback procedures and monitoring

COMPREHENSIVE VALIDATION:
- Load testing under realistic market conditions
- Stress testing with extreme market scenarios
- Integration testing with full production infrastructure
- Performance regression testing against baselines
- Safety testing with fault injection and error scenarios

PRODUCTION DEPLOYMENT CRITERIA:
```cpp
// Pre-production validation framework
namespace production_readiness {

class ProductionValidator {
public:
    struct ValidationResults {
        bool performance_validated;
        bool safety_validated;
        bool integration_validated;
        bool monitoring_ready;
        bool rollback_tested;
        std::vector<std::string> issues;
    };
    
    ValidationResults validate_for_production() {
        ValidationResults results{};
        
        // ✓ Performance validation
        results.performance_validated = validate_performance_requirements();
        
        // ✓ Safety validation  
        results.safety_validated = validate_safety_mechanisms();
        
        // ✓ Integration validation
        results.integration_validated = validate_system_integration();
        
        // ✓ Monitoring readiness
        results.monitoring_ready = validate_monitoring_setup();
        
        // ✓ Rollback testing
        results.rollback_tested = validate_rollback_procedures();
        
        return results;
    }
    
private:
    bool validate_performance_requirements() {
        // Comprehensive latency testing under load
        // Throughput validation with sustained operation
        // Resource utilization within acceptable bounds
        // No performance regressions detected
        return true;  // Implementation-specific validation
    }
};

} // namespace production_readiness
```

RISK ASSESSMENT:
- Impact analysis of potential failures
- Risk mitigation strategy validation
- Emergency procedures and contact information
- Monitoring and alerting threshold configuration
- Rollback trigger criteria and procedures

DOCUMENTATION AND COMPLIANCE:
- Production deployment documentation complete
- Change management approval and sign-off
- Monitoring and alerting configuration documented
- Performance baseline establishment and documentation
- Post-deployment validation procedures defined

MONITORING AND OBSERVABILITY:
- Comprehensive logging for all critical operations
- Performance metrics collection and alerting
- Error rate monitoring with appropriate thresholds
- Business metrics tracking (P&L, position, risk)
- System health monitoring and dashboards

SUCCESS CRITERIA:
- All [VALIDATION_REQUIREMENTS] met with documented evidence
- Production monitoring and alerting fully configured
- Rollback procedures tested and validated
- Risk assessment completed with mitigation strategies
- Deployment approval obtained from stakeholders

Provide comprehensive production readiness assessment with deployment recommendation and risk analysis.
```

## Template Usage Guidelines

### Review Type Selection
- **Performance Review**: For latency-critical components requiring sub-microsecond performance
- **Safety Review**: For trading logic, risk management, and position handling components
- **Architecture Review**: For component integration and system design validation
- **Domain Review**: For Treasury calculation accuracy and market convention compliance
- **Pre-Production Review**: For final validation before production deployment

### Customization Best Practices
- **Specify exact requirements** for all [PARAMETER] placeholders
- **Include specific latency targets** and performance requirements
- **Define risk tolerance** and safety requirements clearly
- **Specify integration context** with existing infrastructure
- **Include compliance requirements** relevant to your organization

### Review Process Integration
- **Use with appropriate agent**: `@claude/agents/hft-code-reviewer.md` for specialized expertise
- **Combine templates**: Use multiple templates for comprehensive reviews
- **Document findings**: Create artifacts with review results and recommendations
- **Track improvements**: Measure before/after performance and safety metrics

### Quality Standards
- **Objective criteria**: Use measurable standards for all evaluations
- **Evidence-based**: Require testing and measurement to support conclusions
- **Action-oriented**: Provide specific, implementable recommendations
- **Risk-aware**: Consider financial and operational risk in all recommendations

These templates provide comprehensive code review approaches tailored to the unique requirements of HFT systems while maintaining the high performance and safety standards required for financial trading applications.
