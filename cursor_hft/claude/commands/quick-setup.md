---
name: quick-setup
description: Fast setup for new Claude Code sessions. Rapidly prime Claude Code with essential HFT project context, display current status, show available agents and commands, and prepare for immediate productive development work.
---

Rapidly initialize Claude Code for HFT trading system development. This command provides essential context, current status, and available tools for immediate productivity.

## Quick Setup Process

### Phase 1: Essential Context Loading (Fast)

**Load core project understanding:**

1. **Project Status Overview** (from README.md summary)
   - Current phase: Phase 3 Complete → Phase 4 Ready
   - Key achievement: 667ns tick-to-trade (22.5x better than targets)
   - Infrastructure status: All components validated and exceeding targets
   - Development readiness: Ready for parallel multi-agent strategy development

2. **Performance Baseline** (from docs/PERFORMANCE_REPORT.md key metrics)
   - Tick-to-trade latency: 667ns median (target <15μs)
   - Strategy decisions: 704ns (target <2μs)  
   - Feed handler: 1.98M msgs/sec (target >1M)
   - Infrastructure: All components exceed targets by 3-21x margins

3. **Proven Patterns** (from Claude.md integration requirements)
   - Object pools: Zero allocation, 14ns operations
   - Ring buffers: Lock-free messaging, 31.8ns latency
   - Timing framework: Comprehensive measurement, 16.6ns overhead
   - Cache alignment: ARM64 64-byte boundaries mandatory

### Phase 2: Development Environment Status

**Display current capabilities:**

```
HFT Trading System - Quick Setup Complete
=========================================

📊 PROJECT STATUS: Phase 3 Complete → Phase 4 Ready ✅
├── Infrastructure: World-class performance validated
├── Trading System: Sub-microsecond pipeline operational  
├── Strategy Framework: 704ns decision latency achieved
├── Development Tools: Automated benchmarking & validation
└── Next Phase: Parallel multi-agent strategy development

⚡ PERFORMANCE SUMMARY: 22.5x Better Than Targets ✅
├── End-to-End: 667ns tick-to-trade (target <15μs)
├── Strategies: 704ns decisions (target <2μs) 
├── Feed Handler: 1.98M msgs/sec (target >1M)
├── Infrastructure: All components exceed targets
└── Available Headroom: 14.3μs (95.6% unused capacity)

🏗️ PROVEN PATTERNS (Must Follow):
├── Object Pools: Zero allocation, 14ns operations
├── Ring Buffers: Lock-free messaging, 31.8ns latency
├── Timing Framework: 16.6ns overhead, comprehensive measurement
├── Cache Alignment: ARM64 64-byte boundaries
└── Treasury Integration: 32nd pricing, yield calculations

💻 DEVELOPMENT INFRASTRUCTURE: Production-Ready ✅
├── Automated Benchmarking: 9 suites, 28 metrics validated
├── Regression Detection: Baseline comparison operational
├── Build System: 100% test pass rate, CMakeLists.txt automation
├── Performance Monitoring: Real-time latency tracking
└── CI/CD Integration: CTest automation working
```

### Phase 3: Available Development Tools

**Show Claude Code capabilities:**

```
🤖 AVAILABLE AGENTS: 4 HFT Specialists Ready
=============================================

@claude/agents/hft-code-reviewer.md
├── Purpose: Comprehensive HFT code review with trading safety focus
├── Expertise: Performance validation, risk assessment, integration compliance
├── Use When: Before merging code, reviewing trading logic, validating patterns
└── Specialties: Latency analysis, memory allocation, trading safety, HFT patterns

@claude/agents/hft-strategy-developer.md  
├── Purpose: Trading strategy development with sub-microsecond performance
├── Expertise: Market making, momentum, mean reversion, statistical arbitrage
├── Use When: Creating strategies, optimizing algorithms, Phase 4 development
└── Specialties: Strategy design, risk integration, performance optimization

@claude/agents/hft-component-builder.md
├── Purpose: Infrastructure component development with zero-allocation design
├── Expertise: Lock-free algorithms, ARM64 optimization, template metaprogramming
├── Use When: Building components, optimizing infrastructure, cache optimization
└── Specialties: Memory management, messaging, data structures, timing systems

@claude/agents/hft-performance-optimizer.md
├── Purpose: Performance analysis with systematic bottleneck identification
├── Expertise: Profiling, regression detection, ARM64 tuning, headroom utilization
├── Use When: Performance issues, optimization planning, regression investigation
└── Specialties: Latency analysis, optimization strategy, headroom allocation

⚙️ AVAILABLE COMMANDS: Development Workflow Automation
====================================================

@claude/commands/prime-project-context.md ✅ Tested Working
├── Purpose: Comprehensive project context loading from existing docs
├── Use When: Starting new sessions, need complete project understanding
└── Result: Full context of project status, architecture, and patterns

@claude/commands/parallel-code-review.md ✅ Ready
├── Purpose: Multi-agent code review with specialized perspectives  
├── Use When: Reviewing code changes, pre-merge validation, quality assurance
└── Agents: Performance, Risk/Safety, Architecture, HFT Domain specialists

@claude/commands/quick-setup.md ✅ Current Command
├── Purpose: Fast session initialization with essential context
├── Use When: Starting development work, need immediate productivity
└── Result: Essential context, agent overview, ready for development

@claude/commands/parallel-strategy-development.md ⚠️ Coming Soon
├── Purpose: Multi-agent strategy development with different trading profiles
├── Use When: Phase 4 strategy development, tournament frameworks
└── Profiles: Conservative, Aggressive, Quantitative, Adaptive agents
```

### Phase 4: Immediate Development Readiness

**Set session context for productive work:**

```
🚀 READY FOR DEVELOPMENT
========================

CURRENT SESSION CONTEXT:
├── Project primed with essential context
├── Performance baseline established (667ns tick-to-trade)
├── Proven patterns loaded (object pools, ring buffers, timing)
├── All 4 specialized agents available
└── Development commands ready for use

RECOMMENDED NEXT ACTIONS:
├── Code Review: Use @claude/agents/hft-code-reviewer.md for any code changes
├── Strategy Work: Use @claude/agents/hft-strategy-developer.md for Phase 4 development
├── Performance Issues: Use @claude/agents/hft-performance-optimizer.md for optimization
├── Infrastructure: Use @claude/agents/hft-component-builder.md for components
└── Multi-Agent Reviews: Use @claude/commands/parallel-code-review.md for comprehensive analysis

PHASE 4 DEVELOPMENT FOCUS:
├── Parallel Multi-Agent Strategy Development
├── Tournament-Style Strategy Competition  
├── Sophisticated Algorithm Implementation (14.3μs headroom available)
├── Machine Learning Integration (2-5μs budget available)
└── Advanced Risk Management Enhancement

INTEGRATION REQUIREMENTS REMINDER:
├── Zero allocation in hot paths (object pools mandatory)
├── Sub-microsecond latency targets (strategy <1μs, components <500ns)
├── Ring buffer messaging (31.8ns established performance)
├── ARM64 cache alignment (64-byte boundaries)
└── Comprehensive testing (functional + performance + integration)
```

## Usage Examples

### Start New Development Session
```bash
# Quick setup for immediate productivity
@quick-setup

# Then immediately start work:
@claude/agents/hft-strategy-developer.md "Develop momentum strategy for 10Y Treasury"
```

### Development Workflow Integration
```bash
# Morning development startup
@quick-setup
# Review any overnight changes
@claude/commands/parallel-code-review.md --git-diff "HEAD~5..HEAD"
# Continue with strategy development
@claude/agents/hft-strategy-developer.md
```

### Problem Investigation
```bash
# Quick setup + performance analysis
@quick-setup
@claude/agents/hft-performance-optimizer.md "Analyze recent benchmark results"
```

## Command Variations

### Minimal Setup (Ultra-Fast)
```bash
@quick-setup --minimal
# Shows only: status, available agents, immediate next actions
```

### Development Focus Setup
```bash
@quick-setup --focus strategy
# Emphasizes strategy development context and Phase 4 readiness

@quick-setup --focus performance  
# Emphasizes performance context and optimization opportunities

@quick-setup --focus review
# Emphasizes code review and quality assurance context
```

### Verbose Setup (Complete Context)
```bash
@quick-setup --verbose
# Includes additional context from full prime-project-context if needed
```

## Integration with Project Workflow

### Daily Development Routine
1. **Start session**: `@quick-setup`
2. **Choose agent**: Based on work type (strategy/component/review/optimization)
3. **Work productively**: Immediate context for effective development
4. **Review changes**: `@parallel-code-review` before committing

### New Team Member Onboarding
1. **Project overview**: `@quick-setup` provides immediate understanding
2. **Deep dive**: `@prime-project-context` for comprehensive context
3. **Start contributing**: Agents provide specialized guidance

### Context Refresh
1. **Mid-session**: `@quick-setup --minimal` for quick status reminder
2. **After break**: Quick context reload without full priming
3. **Focus shift**: `@quick-setup --focus [area]` for different work types

## Expected Output Summary

**After running quick-setup, you have:**
✅ **Essential project context** - Current status, performance, patterns  
✅ **Agent overview** - 4 specialists with usage guidance  
✅ **Command reference** - Available workflow automation  
✅ **Development readiness** - Immediate productivity context  
✅ **Phase 4 focus** - Parallel strategy development preparation  

**Time to productivity**: <30 seconds vs 2-3 minutes for full priming

**Use case**: When you need to get productive quickly without comprehensive context loading, but with enough information to make informed development decisions and effectively use the specialized agents.

This command bridges the gap between starting from scratch and full project priming, optimizing for immediate development productivity while ensuring access to essential HFT trading system context.
