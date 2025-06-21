# HFT Trading System - Claude Code Development Context

## Project Overview
Building a high-frequency trading system for US Treasury markets using AI-driven development. Comparing Claude Code effectiveness against Cursor for complex financial systems.

**Performance Targets (Non-Negotiable):**
- Tick-to-trade: 5-15 microseconds  
- Order book update: 200-500 nanoseconds
- Market data processing: 1-3 microseconds
- Zero memory allocation in hot paths

**Platform:** MacBook Pro M1 Pro (48GB RAM, macOS Sequoia 15.5)

## Development Workflow: Dual-Session Approach

### Implementation Sessions
**Goal:** Build working components that meet individual performance targets
**Context:** Deep component knowledge, integration patterns, optimization techniques
**Success:** Functional correctness + component-level performance
**Claude Code Role:** Generate complete, production-ready components with comprehensive documentation

### System Analysis Sessions  
**Goal:** Analyze system-wide performance and optimization priorities
**Context:** Broader system understanding, latency budget allocation, production considerations
**Success:** System-level performance understanding + optimization roadmap
**Claude Code Role:** Analyze end-to-end performance and provide optimization strategies

## Current Status
- **Phase 1-2:** Complete (Cursor) - Core infrastructure + market data systems
- **Phase 3:** Current (Claude Code) - Trading logic starting with order book
- **Next Milestone:** Order book implementation (Claude Code validation)

## Proven Infrastructure (Phase 1-2 Success)
- ✅ High-precision timing framework (<100ns overhead)
- ✅ Lock-free SPSC ring buffer (29.7ns latency)
- ✅ Object pools with compile-time optimization
- ✅ Treasury market data structures (32nd pricing, yield calculations)
- ✅ Feed handler framework (<200ns parsing, >1M msgs/sec)
- ✅ Primary dealer venue simulation

## Established Patterns (Must Follow)
- **Single-header components** (proven effective)
- **Object pool integration** (mandatory for zero-allocation)
- **Ring buffer messaging** (established 29.7ns performance)
- **Cache-aligned data structures** (64-byte ARM64)
- **Template-based optimization** (compile-time performance)
- **Treasury market conventions** (32nd fractional pricing)

## Component Integration Requirements
All new components MUST integrate with:
1. **Object pools** for memory management
2. **Ring buffers** for inter-component messaging  
3. **Timing framework** for latency measurement
4. **Treasury data structures** for market data consistency

## Performance Validation Approach
- **Component Level:** Claude Code validates basic performance targets
- **System Level:** Separate analysis session for end-to-end optimization
- **Build Validation:** All tests must pass, CMakeLists.txt must be updated
- **Documentation:** Comprehensive inline documentation for code review

---

# IMPLEMENTATION SESSION CONTEXT

## Code Generation Standards
- **Complete Files:** Generate full headers, implementations, tests, benchmarks
- **Build Integration:** MUST update CMakeLists.txt with new executables
- **Documentation Level:** Comprehensive inline documentation including:
  - Design decisions and rationale
  - Performance optimization explanations
  - Integration complexity details
  - Alternative approaches considered
  - HFT-specific optimizations

## Component Template Pattern
Every component must follow this structure:
```cpp
// 1. Cache-aligned data structures (64-byte ARM64)
// 2. Object pool integration for zero allocation
// 3. Ring buffer messaging compatibility
// 4. Template-based compile-time optimization
// 5. Treasury market data integration
```

## Performance Requirements (Component Level)
- **Latency targets** specified per component
- **Memory allocation:** Zero in hot paths
- **Cache behavior:** Optimize for L1 cache on ARM64
- **Branch prediction:** Optimize common code paths
- **Template specialization:** Use for performance-critical sections

## Error Handling Strategy
- **Auto-iterate:** Build failures, simple test failures, obvious integration issues
- **Escalate after 3-4 attempts:** Complex architectural problems, performance regressions
- **Always validate:** Builds successfully + all tests pass + benchmarks meet targets

## Integration Validation Checklist
- [ ] Integrates with object pools correctly
- [ ] Uses ring buffer messaging patterns
- [ ] Maintains cache alignment requirements
- [ ] Follows treasury market data conventions
- [ ] Zero memory allocation in hot paths verified
- [ ] CMakeLists.txt updated with new executables
- [ ] All existing tests continue to pass

## Test Requirements
1. **Functional Tests:** Core component behavior
2. **Performance Tests:** Latency and throughput validation
3. **Integration Tests:** Compatibility with existing infrastructure
4. **Edge Case Tests:** Error handling and boundary conditions

## Benchmark Requirements
- **Latency measurement** using established timing framework
- **Throughput validation** for data processing components
- **Memory allocation verification** (must be zero in hot paths)
- **Cache behavior analysis** where relevant to performance

---

# SYSTEM ANALYSIS SESSION CONTEXT

## System-Wide Performance Targets
- **End-to-end tick-to-trade:** <15 microseconds
- **Component interaction overhead:** Minimize message passing latency
- **Memory subsystem:** Optimize cache behavior across components
- **Load behavior:** Maintain performance under realistic market data rates

## Latency Budget Allocation
Current component latencies (established):
- Timing framework: <100ns overhead
- Ring buffer messaging: 29.7ns per operation
- Object pool operations: 8-18ns depending on timing
- Treasury yield calculations: 51-104ns
- 32nd price conversions: 22-25ns
- Message parsing: <200ns per message
- Feed handler throughput: >1M messages/second

**Budget remaining for new components:**
- Order book operations: Target <500ns
- Strategy execution: Target <2μs
- Risk calculations: Target <1μs
- Order routing: Target <500ns
- Network/venue communication: Budget varies

## Integration Complexity Analysis
Focus areas for system-wide optimization:
1. **Component interaction patterns:** Ring buffer efficiency across multiple components
2. **Cache coherency:** Memory layout optimization for multi-component workflows
3. **Data transformation overhead:** Minimize conversions between component data formats
4. **Batch processing opportunities:** Optimize for message burst handling
5. **Memory allocation patterns:** Verify zero-allocation guarantee under load

## Load Testing Considerations
- **Market data burst scenarios:** Handle opening/closing bell message rates
- **Strategy coordination:** Multiple strategies operating simultaneously
- **Risk calculation frequency:** Real-time position and P&L updates
- **Network latency variability:** Venue communication under different conditions

## Optimization Priority Framework
1. **Critical path analysis:** Identify bottlenecks in tick-to-trade pipeline
2. **Performance regression detection:** Ensure new components don't degrade existing performance
3. **Scalability assessment:** Component performance under increasing complexity
4. **Production readiness:** System behavior under realistic market conditions

## Analysis Output Requirements
- **Bottleneck identification:** Specific components or interactions limiting performance
- **Optimization recommendations:** Concrete steps to improve system-wide latency
- **Trade-off analysis:** Performance vs complexity vs maintainability
- **Implementation priority:** Which optimizations provide greatest latency reduction
