---
name: prime-project-context
description: Prime Claude Code with comprehensive HFT project context using existing documentation. Loads current status, architecture, performance data, and development patterns from established project files.
---

Load and analyze the complete HFT trading system context to understand project state, architecture, proven patterns, and development standards using existing project documentation.

## Phase 1: Core Project Understanding

**Read foundational project documents:**

1. **`Claude.md`** - Primary development context and proven patterns
   - Current development phase and status (Phase 3 Complete → Phase 4 Ready)
   - Performance achievements vs targets (667ns tick-to-trade, 22.5x better than targets)
   - Proven architectural patterns (object pools, ring buffers, single-header components)
   - Development workflow and integration requirements
   - Session-specific context and current objectives

2. **`README.md`** - Project overview and achievements
   - Project goals and performance targets
   - Current status and phase completion
   - Build instructions and quick start guide
   - Performance summary and achievements
   - Development methodology and AI tool usage

3. **`hft_project_plan.md`** - Detailed project plan and progress tracking
   - Complete phase breakdown and completion status
   - Performance targets vs achievements analysis
   - Development tool effectiveness (Cursor vs Claude Code)
   - Innovation targets and success metrics
   - Phase 4 readiness assessment

## Phase 2: Architecture and Performance Context

**Read technical architecture documentation:**

4. **`docs/ARCHITECTURE.md`** - System design and component relationships
   - Component architecture and integration patterns
   - Performance characteristics and latency budgets
   - Memory subsystem design and optimization
   - Hardware architecture (ARM64) integration
   - Testing and deployment architecture

5. **`docs/PERFORMANCE_REPORT.md`** - Comprehensive performance analysis
   - Detailed performance metrics and validation
   - Component-level performance breakdown
   - Optimization case studies and techniques
   - Production readiness assessment
   - Performance engineering methodologies

6. **`docs/AUTOMATED_BENCHMARKS.md`** - Benchmark system and validation
   - Automated benchmark execution and validation
   - Performance baseline management
   - Regression detection methodology
   - Development workflow integration
   - Troubleshooting and best practices

## Phase 3: Development Standards and Patterns

**Analyze established patterns from codebase:**

7. **Project structure analysis:**
   ```bash
   # Get comprehensive project overview
   tree -I 'build|.git' -L 3
   
   # Analyze component organization
   tree include/hft/ -L 2
   tree tests/ -L 2
   tree benchmarks/ -L 2
   ```

8. **Code pattern analysis:**
   - Review header-only component implementations
   - Understand object pool integration patterns
   - Analyze ring buffer messaging usage
   - Examine timing framework integration
   - Study treasury data structure conventions

## Phase 4: Current Implementation Status

**Extract current system capabilities:**

9. **Infrastructure status (from documentation):**
   - Timing framework: 16.6ns overhead (target <100ns, 6x better)
   - Object pools: 14ns operations (target <25ns, 1.8x better)
   - Ring buffers: 31.8ns latency (target <50ns, meets target)
   - Feed handler: 1.98M msgs/sec (target >1M, 98% better)

10. **Trading system status (validated performance):**
    - End-to-end tick-to-trade: 667ns (target <15μs, 22.5x better)
    - Strategy decisions: 704ns (target <2μs, 3x better)
    - Order book operations: 59-1487ns (targets met/exceeded)
    - Risk management: Sub-microsecond calculations validated

11. **Development infrastructure status:**
    - Automated benchmarking: 9 benchmark suites, 28 metrics validated
    - Regression detection: Working baseline comparison system
    - Build integration: 100% test pass rate, CMakeLists.txt automation
    - Performance monitoring: Real-time latency tracking operational

## Phase 5: Development Readiness Assessment

**Generate comprehensive context summary:**

### Project Status Summary
```
HFT Trading System - Development Context Loaded
===============================================

CURRENT PHASE: Phase 3 Complete → Phase 4 Ready (Advanced Strategy Development)

PERFORMANCE STATUS: WORLD-CLASS ✅
├── Tick-to-trade latency: 667ns (target <15μs, 22.5x better)
├── Feed handler throughput: 1.98M msgs/sec (target >1M, 98% better)
├── Strategy performance: 704ns decisions (target <2μs, 3x better)
├── Infrastructure: All components exceed targets by 3-21x margins
└── Validation: 28/28 metrics successfully tested

DEVELOPMENT INFRASTRUCTURE: PRODUCTION-READY ✅
├── Automated benchmarking: 9 benchmark suites operational
├── Regression detection: Baseline comparison working
├── Build system: 100% test pass rate with CMakeLists.txt automation
├── Performance monitoring: Real-time latency tracking
└── Development workflow: Proven patterns and integration requirements

ARCHITECTURAL FOUNDATION: VALIDATED ✅
├── Object pools: 14ns operations, zero allocation guaranteed
├── Ring buffers: 31.8ns messaging, lock-free communication
├── Timing framework: 16.6ns overhead, comprehensive measurement
├── Treasury structures: 32nd pricing, yield calculations validated
└── ARM64 optimization: Cache-aligned, 64-byte boundaries
```

### Proven Development Patterns (MUST FOLLOW)
```cpp
// Mandatory integration pattern for all new components
#include "hft/timing/hft_timer.hpp"
#include "hft/memory/object_pool.hpp"
#include "hft/messaging/spsc_ring_buffer.hpp"

// Object pool integration (zero allocation)
using ComponentPool = ObjectPool<ComponentType, 4096, false>;

// Ring buffer messaging (31.8ns latency)
using ComponentBuffer = SPSCRingBuffer<ComponentMessage, 8192>;

// Timing framework integration (16.6ns overhead)
auto start = hft::HFTTimer::get_cycles();
// ... component operation ...
auto latency = hft::HFTTimer::cycles_to_ns(hft::HFTTimer::get_cycles() - start);

// Cache alignment for ARM64 (mandatory)
struct alignas(64) ComponentData { /* hot data first */ };
```

### Performance Requirements by Component Type
```
Component Latency Targets (All Validated):
├── Strategy Logic: <2μs (achieved: 704ns, 3x better)
├── Risk Management: <1μs (achieved: <704ns, excellent)
├── Order Management: <500ns (achieved: 59-1487ns, meets/exceeds)
├── Market Data: <200ns (achieved: 112ns avg, 27x better)
├── Infrastructure: <100ns (achieved: varies, all targets met)
└── Memory Allocation: Zero in hot paths (verified across all components)
```

### Development Standards (Established)
```
Code Quality Requirements:
├── Single-header components with template optimization
├── Object pool integration for zero-allocation memory management
├── Ring buffer messaging for inter-component communication
├── HFTTimer integration for comprehensive latency measurement
├── Cache-aligned structures for ARM64 64-byte optimization
├── Comprehensive testing (unit + performance + integration)
├── CMakeLists.txt integration with proper linking
└── Build validation with 100% test pass rate requirement
```

### Phase 4 Development Context
```
READY FOR: Parallel Multi-Agent Strategy Development 🚀

Available Infrastructure:
├── Complete validated foundation with 22.5x performance margins
├── Sophisticated strategy framework with 704ns decision latency
├── Comprehensive risk management with sub-microsecond calculations
├── Automated performance validation with 28 metrics
└── Development workflow with proven patterns and automation

Performance Headroom Available:
├── Latency budget: 14.333μs unused (95.6% of 15μs target)
├── Strategy complexity: Massive headroom for sophisticated algorithms
├── Multi-agent coordination: <500ns overhead target feasible
├── ML integration: 2-5μs budget available for learning algorithms
└── Advanced risk models: 1-2μs budget for complex calculations

Development Priorities:
1. Leverage performance headroom for sophisticated trading strategies
2. Implement parallel multi-agent strategy development framework
3. Create tournament-style strategy competition and optimization
4. Maintain world-class performance while adding algorithm complexity
5. Focus on strategy sophistication over micro-optimizations
```

### Integration Requirements (Non-Negotiable)
```
All New Development Must:
├── Use object pools for memory management (zero allocation)
├── Integrate with ring buffer messaging (31.8ns latency)
├── Include HFTTimer for latency measurement (16.6ns overhead)
├── Follow cache alignment patterns (64-byte ARM64)
├── Work with treasury data structures (32nd pricing, yields)
├── Include comprehensive testing (functional + performance)
├── Update CMakeLists.txt with proper linking
├── Validate against performance targets
└── Maintain compatibility with existing proven patterns
```

## Expected Output

**Comprehensive development readiness summary covering:**

1. **Project Status**: Phase 3 Complete, Phase 4 Ready with validated world-class performance
2. **Architecture Understanding**: Component relationships, proven patterns, integration requirements
3. **Performance Context**: 667ns tick-to-trade baseline, 95.6% headroom available, all targets exceeded
4. **Development Standards**: Single-header components, object pools, ring buffers, comprehensive testing
5. **Immediate Readiness**: Ready for parallel multi-agent strategy development with sophisticated algorithms

**Key Takeaway**: System demonstrates world-class performance (22.5x better than targets) with massive headroom (95.6% unused capacity) ready for sophisticated Phase 4 multi-agent strategy development.

This priming ensures Claude Code has complete context from existing documentation for effective HFT development work while leveraging the proven patterns and exceptional performance foundation that has been established.
