---
name: parallel-code-review
description: Orchestrate comprehensive code reviews using multiple specialized agents with different perspectives. Provides thorough analysis through performance, risk, architecture, and HFT domain expertise with synthesis and conflict resolution.
---

Conduct comprehensive code review using multiple specialized agents for thorough analysis of HFT trading system components. This command coordinates parallel reviews and synthesizes findings into actionable recommendations.

## Multi-Agent Review Strategy

### Review Agent Coordination

**Primary Reviewers (Run in Parallel):**

#### Agent 1: Performance Specialist (@hft-performance-optimizer)
**Focus**: Ultra-low latency and efficiency optimization
**Prompt**: "Review this code focusing exclusively on performance characteristics. Analyze:
- Latency implications and timing critical paths
- Memory allocation patterns (zero allocation enforcement)
- Cache efficiency and ARM64 optimization opportunities  
- Branch prediction and hot path optimization
- Integration with existing performance infrastructure (object pools, ring buffers, timing framework)
Ignore style issues unless they impact performance. Provide specific optimization recommendations with expected impact."

#### Agent 2: Risk & Safety Specialist (@hft-code-reviewer + Risk Focus)
**Focus**: Trading safety and correctness
**Prompt**: "Review this code focusing exclusively on trading risk and safety. Analyze:
- Position management and risk limit enforcement
- P&L calculation accuracy and mathematical correctness
- Order lifecycle management and state consistency
- Error handling and fault tolerance
- Compliance and audit trail requirements
- Market data integrity and precision
Ignore performance micro-optimizations unless they impact trading safety. Focus on preventing financial loss or market disruption."

#### Agent 3: Architecture & Integration Specialist (@hft-component-builder + Architecture Focus)  
**Focus**: Design patterns and system integration
**Prompt**: "Review this code focusing exclusively on architectural concerns. Analyze:
- Component integration patterns and consistency with proven designs
- Object pool and ring buffer usage compliance
- Design consistency with established HFT patterns
- Code maintainability and extensibility
- CMakeLists.txt integration and build system compliance
- API design and interface consistency
Ignore micro-optimizations. Focus on long-term maintainability and integration with existing infrastructure."

#### Agent 4: HFT Domain Specialist (@hft-strategy-developer + Domain Focus)
**Focus**: Trading domain expertise and market microstructure
**Prompt**: "Review this code focusing exclusively on HFT trading domain concerns. Analyze:
- Market microstructure handling and trading logic correctness
- Treasury market specifics (32nd pricing, yield calculations, settlement)
- Strategy implementation patterns and trading algorithm correctness
- Order management and execution logic
- Market data processing and feed handling
- Production trading environment considerations
Focus on trading-specific expertise that general software engineers might miss."

## Review Orchestration Process

### Phase 1: Parallel Review Execution

**Step 1: Code Analysis Preparation**
```bash
# Identify review scope
FILES_TO_REVIEW="[specified files or git diff]"
REVIEW_TYPE="[component/strategy/integration/optimization]"
FOCUS_AREAS="[performance/safety/architecture/domain]"

# Prepare context for each agent
PERFORMANCE_CONTEXT="Focus on latency targets: <2μs strategies, <500ns components, zero allocation"
RISK_CONTEXT="Focus on trading safety: position limits, P&L accuracy, error handling"
ARCHITECTURE_CONTEXT="Focus on integration: object pools, ring buffers, proven patterns"
DOMAIN_CONTEXT="Focus on HFT expertise: market microstructure, treasury markets, trading logic"
```

**Step 2: Concurrent Agent Reviews**
Execute 4 parallel reviews with specialized prompts:

1. **Performance Review** (Agent 1):
   - Load `@hft-performance-optimizer`
   - Apply performance-focused prompt
   - Analyze latency, memory, cache, ARM64 optimization

2. **Risk Review** (Agent 2):
   - Load `@hft-code-reviewer` with risk focus
   - Apply safety-focused prompt  
   - Analyze trading safety, correctness, compliance

3. **Architecture Review** (Agent 3):
   - Load `@hft-component-builder` with architecture focus
   - Apply integration-focused prompt
   - Analyze design patterns, maintainability, integration

4. **Domain Review** (Agent 4):
   - Load `@hft-strategy-developer` with domain focus
   - Apply HFT domain-focused prompt
   - Analyze trading logic, market specifics, domain expertise

### Phase 2: Review Synthesis and Analysis

**Step 3: Collect and Categorize Findings**
```
Review Aggregation:
├── Critical Issues (any agent flags as blocking)
│   ├── Performance violations (latency, memory allocation)
│   ├── Safety violations (trading risk, financial correctness)
│   ├── Architecture violations (integration, patterns)
│   └── Domain violations (trading logic, market handling)
├── Consensus Concerns (multiple agents identify)
│   ├── Cross-cutting issues affecting multiple domains
│   ├── Integration problems spanning components
│   └── Design inconsistencies noted by multiple reviewers
├── Specialized Insights (single agent expertise)
│   ├── Performance optimization opportunities
│   ├── Risk management enhancements
│   ├── Architectural improvements
│   └── Domain-specific optimizations
└── Conflicting Recommendations (require resolution)
    ├── Performance vs Safety trade-offs
    ├── Short-term vs Long-term optimization
    └── Implementation approach disagreements
```

**Step 4: Conflict Resolution Framework**
Apply HFT-specific prioritization rules:

```cpp
// Conflict resolution priority matrix
namespace conflict_resolution {
    enum class Priority : uint8_t {
        TradingSafety = 1,      // Highest: Prevent financial loss
        PerformanceTargets = 2,  // High: Meet HFT latency requirements  
        SystemIntegration = 3,   // Medium: Maintain proven patterns
        CodeMaintainability = 4  // Low: Long-term considerations
    };
    
    // Resolution rules
    struct ResolutionRules {
        // Trading safety always wins
        static constexpr bool SAFETY_OVERRIDES_PERFORMANCE = true;
        
        // Performance targets must be met for HFT
        static constexpr bool PERFORMANCE_OVERRIDES_STYLE = true;
        
        // Proven patterns preferred over innovation
        static constexpr bool PROVEN_PATTERNS_PREFERRED = true;
        
        // Domain expertise wins on trading-specific decisions
        static constexpr bool HFT_EXPERT_WINS_DOMAIN_CONFLICTS = true;
    };
}
```

### Phase 3: Master Review Generation

**Step 5: Generate Consolidated Review**

```
HFT Parallel Code Review Summary
================================

REVIEW SCOPE: [files/components reviewed]
AGENTS CONSULTED: Performance, Risk/Safety, Architecture, HFT Domain

🚨 CRITICAL ISSUES (Must fix before merge)
[Blocking issues identified by any reviewer - sorted by business impact]

├── Trading Safety Violations:
│   └── [Risk specialist findings requiring immediate attention]
├── Performance Violations:  
│   └── [Performance specialist findings exceeding latency budgets]
├── Integration Violations:
│   └── [Architecture specialist findings breaking proven patterns]
└── Domain Logic Errors:
    └── [HFT specialist findings in trading logic or market handling]

⚠️ CONSENSUS CONCERNS (Multiple reviewers identified)
[Issues flagged by 2+ specialized reviewers - indicates systemic problems]

🎯 PERFORMANCE ANALYSIS
├── Latency Impact: [Specific latency analysis and optimization opportunities]
├── Memory Discipline: [Allocation behavior and object pool compliance]
├── Cache Efficiency: [ARM64 optimization and memory access patterns]
└── Integration Overhead: [Component interaction performance]

🛡️ RISK MANAGEMENT ASSESSMENT  
├── Trading Safety: [Position management, risk limits, P&L accuracy]
├── Error Handling: [Fault tolerance and graceful degradation]
├── Compliance: [Audit trails and regulatory requirements]
└── Production Readiness: [Operational robustness and monitoring]

🏗️ ARCHITECTURAL EVALUATION
├── Design Consistency: [Adherence to proven HFT patterns]
├── Integration Compliance: [Object pools, ring buffers, timing framework]
├── Maintainability: [Code organization and long-term evolution]
└── Build System: [CMakeLists.txt integration and dependencies]

📈 HFT DOMAIN INSIGHTS
├── Market Microstructure: [Trading logic and market handling analysis]
├── Treasury Specifics: [32nd pricing, yield calculations, settlement handling]
├── Production Trading: [Real-world trading environment considerations]  
└── Strategy Implementation: [Trading algorithm correctness and optimization]

CONFLICT RESOLUTION:
[Document any conflicts between reviewers and resolution rationale]

PERFORMANCE IMPACT ASSESSMENT:
├── Latency: [IMPROVES/MAINTAINS/DEGRADES] baseline metrics
├── Throughput: [IMPROVES/MAINTAINS/DEGRADES] processing capacity
├── Memory: [COMPLIANT/VIOLATIONS] with zero-allocation discipline
└── Integration: [COMPLIANT/ISSUES] with existing infrastructure

RISK ASSESSMENT:
├── Trading Safety: [SAFE/REVIEW_REQUIRED/UNSAFE] for production
├── Financial Impact: [LOW/MEDIUM/HIGH] potential for loss
├── Operational Risk: [LOW/MEDIUM/HIGH] system stability impact
└── Compliance: [COMPLIANT/GAPS/VIOLATIONS] with requirements

OVERALL RECOMMENDATION:
├── Decision: [APPROVE/CHANGES_REQUIRED/ARCHITECTURE_REVIEW/UNSAFE]
├── Priority: [LOW/MEDIUM/HIGH/CRITICAL] for addressing issues
├── Timeline: [Estimated effort for addressing concerns]
└── Follow-up: [Specific next steps and validation requirements]

AGENT CONSENSUS MATRIX:
├── Performance Specialist: [APPROVE/CONDITIONAL/REJECT]
├── Risk Specialist: [APPROVE/CONDITIONAL/REJECT]  
├── Architecture Specialist: [APPROVE/CONDITIONAL/REJECT]
└── HFT Domain Specialist: [APPROVE/CONDITIONAL/REJECT]
```

## Usage Examples

### Basic Component Review
```bash
# Review specific files
@parallel-code-review --files "include/hft/trading/new_strategy.hpp tests/strategy/new_strategy_test.cpp"

# Review git diff
@parallel-code-review --git-diff "main..feature-branch"
```

### Focused Review Types
```bash
# Performance-critical review
@parallel-code-review --focus performance --files "include/hft/trading/order_book.hpp"

# Safety-critical review  
@parallel-code-review --focus safety --files "include/hft/risk/position_manager.hpp"

# Integration review
@parallel-code-review --focus integration --files "include/hft/messaging/new_component.hpp"
```

### Full System Review
```bash
# End-to-end pipeline review
@parallel-code-review --scope system --components "feed_handler,order_book,strategy_engine"

# Pre-deployment review
@parallel-code-review --scope production --comprehensive
```

## Command Configuration

### Review Scope Options
- `--files [file_list]` - Specific files to review
- `--git-diff [branch_range]` - Git diff range to review
- `--components [component_list]` - Logical components to review
- `--scope [component|integration|system|production]` - Review comprehensiveness

### Focus Options  
- `--focus [performance|safety|architecture|domain|all]` - Emphasize specific aspects
- `--agents [agent_list]` - Use specific subset of agents
- `--depth [quick|standard|comprehensive]` - Review thoroughness level

### Output Options
- `--format [summary|detailed|actionable]` - Output detail level
- `--conflicts-only` - Show only conflicting recommendations
- `--critical-only` - Show only critical issues

## Integration with Development Workflow

### Pre-Commit Integration
```bash
# Automated review for commits
git add .
@parallel-code-review --git-diff "HEAD~1..HEAD" --focus critical
# Review results inform commit decision
```

### Pull Request Integration
```bash
# Comprehensive PR review
@parallel-code-review --git-diff "main..pr-branch" --scope integration --format detailed
# Results inform PR approval/changes
```

### Performance Validation
```bash
# Validate performance impact
@parallel-code-review --focus performance --components changed_components
# Ensure no regressions in latency-critical paths
```

This parallel review process ensures comprehensive coverage while leveraging specialized expertise for thorough HFT code analysis with actionable, prioritized recommendations.
