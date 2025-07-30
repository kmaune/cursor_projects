# Claude Code Commands Directory

This directory contains workflow automation commands designed specifically for HFT trading system development. These commands orchestrate Claude Code agents, automate common development tasks, and provide structured workflows for complex multi-agent development scenarios.

## Available Commands

### Core Workflow Commands

#### `prime-project-context.md` ✅ **Tested Working**
**Purpose**: Comprehensive project context loading from existing documentation  
**Use Case**: Starting new Claude Code sessions with full project understanding  

**What it does**:
- Reads foundational docs (`Claude.md`, `README.md`, `hft_project_plan.md`)
- Loads architecture context (`docs/ARCHITECTURE.md`, `docs/PERFORMANCE_REPORT.md`)
- Analyzes project structure and established patterns
- Generates complete development readiness summary with current status

**Usage**:
```bash
@claude/commands/prime-project-context.md
```

**Output**: Complete context including project phase, performance achievements, proven patterns, and development standards.

---

#### `quick-setup.md` ✅ **Ready**
**Purpose**: Fast session initialization with essential context  
**Use Case**: Daily development startup when you need productivity quickly  

**What it does**:
- Loads essential project status (Phase 3 Complete → Phase 4 Ready)
- Shows key performance metrics (667ns tick-to-trade, 22.5x better than targets)
- Displays available agents and commands
- Provides immediate development context and next actions

**Usage**:
```bash
@quick-setup
@quick-setup --minimal        # Ultra-fast essential info only
@quick-setup --focus strategy # Emphasize strategy development context
```

**Time to productivity**: <30 seconds vs 2-3 minutes for full priming

---

#### `parallel-code-review.md` ✅ **Ready**
**Purpose**: Multi-agent code review orchestration  
**Use Case**: Comprehensive code review with specialized perspectives  

**What it does**:
- Coordinates 4 specialized reviewers (Performance, Risk/Safety, Architecture, HFT Domain)
- Runs parallel reviews with focused prompts
- Synthesizes findings with conflict resolution
- Generates consolidated recommendations with priority matrix

**Usage**:
```bash
@parallel-code-review --files "include/hft/strategy/new_strategy.hpp"
@parallel-code-review --git-diff "main..feature-branch"
@parallel-code-review --focus performance --files "order_book.hpp"
```

**Output**: Comprehensive review with critical issues, consensus concerns, and actionable recommendations.

---

#### `parallel-strategy-development.md` ✅ **Ready** 🏆 **Advanced Workflow**
**Purpose**: Multi-agent strategy development with git worktree isolation  
**Use Case**: Phase 4 parallel strategy development with independent agents  

**Detailed Documentation Below** ⬇️

## Parallel Strategy Development - Detailed Guide

### Overview

The `parallel-strategy-development` command orchestrates multiple Claude Code agents to develop trading strategies simultaneously in isolated git worktrees. This enables conflict-free development, independent testing, and tournament-style strategy selection.

### Architecture

#### Git Worktree Structure
```
Main Repository: /Users/you/cursor_hft/
├── Standard development workspace
└── Parallel strategy worktrees:
    ├── ../hft-strategies/hft-conservative/     # Agent 1 workspace
    ├── ../hft-strategies/hft-aggressive/       # Agent 2 workspace  
    ├── ../hft-strategies/hft-quantitative/     # Agent 3 workspace
    └── ../hft-strategies/hft-adaptive/         # Agent 4 workspace
```

Each worktree:
- **Independent filesystem** - No file conflicts between agents
- **Separate git branch** - `strategy/conservative-20250129`, etc.
- **Own build environment** - Independent CMake builds and testing
- **Isolated development** - Agents can iterate without interference

#### Agent Profiles

**1. Conservative Market Maker Agent**
```
Worktree: ../hft-strategies/hft-conservative
Branch: strategy/conservative-YYYYMMDD
Objective: Risk-adjusted returns, stability focus
Targets:
├── Max drawdown: <2%
├── Sharpe ratio: >2.0  
├── Position holding: <10 seconds
├── Risk tolerance: Low
└── Strategy focus: Market making with inventory management
```

**2. Aggressive Momentum Agent**
```
Worktree: ../hft-strategies/hft-aggressive
Branch: strategy/aggressive-YYYYMMDD
Objective: Maximum absolute returns, volatility tolerance
Targets:
├── Return per trade: >0.01%
├── Max position: 50% of limit
├── Holding time: 1-30 seconds
├── Risk tolerance: High
└── Strategy focus: Trend following and breakout capture
```

**3. Quantitative Statistical Arbitrage Agent**
```
Worktree: ../hft-strategies/hft-quantitative
Branch: strategy/quantitative-YYYYMMDD
Objective: Model-driven signals, high win rate
Targets:
├── Win rate: >70%
├── Holding time: <5 seconds
├── Model confidence: High thresholds
├── Risk tolerance: Medium
└── Strategy focus: Statistical relationships and mean reversion
```

**4. Adaptive Learning Agent**
```
Worktree: ../hft-strategies/hft-adaptive
Branch: strategy/adaptive-YYYYMMDD
Objective: Real-time learning and parameter optimization
Targets:
├── Learning frequency: Real-time
├── Parameter adaptation: Dynamic
├── Performance feedback: Continuous
├── Risk tolerance: Dynamic
└── Strategy focus: ML-enhanced algorithms with regime detection
```

### Usage Workflow

#### Phase 1: Environment Setup
```bash
# Create isolated worktrees for parallel development
@parallel-strategy-development --setup

# Expected output:
✅ Created isolated worktrees for parallel development
🤖 Conservative Agent: ../hft-strategies/hft-conservative
🤖 Aggressive Agent: ../hft-strategies/hft-aggressive
🤖 Quantitative Agent: ../hft-strategies/hft-quantitative
🤖 Adaptive Agent: ../hft-strategies/hft-adaptive
🚀 All agent environments prepared!
```

**What happens**:
1. Creates 4 git worktrees from current main branch
2. Each worktree gets unique branch name with timestamp
3. Sets up independent build environments (CMake + build directories)
4. Creates agent execution scripts with specialized prompts
5. Configures isolated development environments

#### Phase 2: Agent Development Launch
```bash
# Manual launch approach (recommended for control):

# Terminal 1: Conservative Agent
cd ../hft-strategies/hft-conservative
./run_agent.sh
# Launches Claude Code with conservative market making prompt

# Terminal 2: Aggressive Agent  
cd ../hft-strategies/hft-aggressive
./run_agent.sh
# Launches Claude Code with aggressive momentum prompt

# Terminal 3: Quantitative Agent
cd ../hft-strategies/hft-quantitative
./run_agent.sh
# Launches Claude Code with statistical arbitrage prompt

# Terminal 4: Adaptive Agent
cd ../hft-strategies/hft-adaptive
./run_agent.sh
# Launches Claude Code with adaptive learning prompt
```

**Agent Execution Script (`run_agent.sh`)**:
```bash
#!/bin/bash
# Generated for each worktree

# Load project context in this worktree
@claude/commands/prime-project-context.md

# Activate strategy development agent
@claude/agents/hft-strategy-developer.md

# Display agent-specific prompt
echo "Agent Profile: conservative"
echo "Development Objectives: Risk-adjusted returns, <2% drawdown, Sharpe >2.0"
echo "Strategy Focus: Market making with sophisticated inventory management"
echo ""
echo "Ready for strategy development with full HFT infrastructure context..."
```

#### Phase 3: Development Monitoring
```bash
# Monitor progress across all agents
@parallel-strategy-development --status

# Expected output:
📊 Strategy Development Status
==============================

🤖 Conservative Agent Status:
  ├── Worktree: ../hft-strategies/hft-conservative
  ├── Branch: strategy/conservative-20250129
  ├── Commits: 12
  ├── Files changed: 8
  ├── Strategy implementation: ✅ Present
  ├── Unit tests: ✅ Present
  ├── Benchmarks: ✅ Present
  └── Build status: ✅ Passing

🤖 Aggressive Agent Status:
  ├── Worktree: ../hft-strategies/hft-aggressive
  ├── Branch: strategy/aggressive-20250129
  ├── Commits: 8
  ├── Files changed: 6
  ├── Strategy implementation: ✅ Present
  ├── Unit tests: ⚠️  Not found
  ├── Benchmarks: ✅ Present
  └── Build status: ❌ Failing

# ... status for quantitative and adaptive agents
```

**Monitoring Features**:
- **Git status**: Branch, commit count, files changed
- **Implementation status**: Strategy files, tests, benchmarks
- **Build validation**: Compilation and test status
- **Development progress**: Objective completion tracking

#### Phase 4: Tournament Framework
```bash
# Run strategy tournament when development is complete
@parallel-strategy-development --tournament

# Expected output:
🏆 Strategy Tournament Framework
===============================

✅ Found conservative strategy implementation
✅ Found aggressive strategy implementation  
✅ Found quantitative strategy implementation
⚠️  adaptive strategy not ready for tournament

🏁 Tournament Participants: conservative aggressive quantitative
📊 Running comparative analysis...

Tournament Results:
├── Conservative Strategy: 85/100 (Risk-adjusted score)
├── Aggressive Strategy: 78/100 (Return-based score)
├── Quantitative Strategy: 92/100 (Consistency score)
└── Adaptive Strategy: Not ready

📋 Tournament framework created in ./tournament_results_20250129_143022
```

**Tournament Analysis**:
- **Performance scoring** across multiple dimensions
- **Comparative benchmarking** with standardized metrics
- **Risk-adjusted evaluation** for different strategy types
- **Selection criteria** based on configurable thresholds

#### Phase 5: Strategy Selection and Integration
```bash
# Merge winning strategies (score >= 80, max 3 strategies)
@parallel-strategy-development --merge

# Expected output:
🏆 Strategy Selection and Merge
==============================

📊 Strategy Performance Scores:
  ✅ conservative: 85/100 (PASS)
  ❌ aggressive: 78/100 (FAIL)
  ✅ quantitative: 92/100 (PASS)
  ✅ adaptive: 88/100 (PASS)

🏆 Selected strategies: quantitative conservative adaptive

🔄 Merging winning strategies to main branch:
  📥 Merging quantitative strategy...
      Branch: strategy/quantitative-20250129
      ✅ Merge successful
  📥 Merging conservative strategy...
      Branch: strategy/conservative-20250129  
      ✅ Merge successful
  📥 Merging adaptive strategy...
      Branch: strategy/adaptive-20250129
      ✅ Merge successful

🎉 Strategy integration complete!
📊 New strategies available in main branch
```

**Merge Process**:
- **Automatic scoring** based on tournament results
- **Selection criteria** (minimum score, maximum strategies)
- **Git merge workflow** with detailed commit messages
- **Integration validation** and conflict resolution

### Advanced Usage

#### Custom Configuration
```bash
# Custom agent profiles
@parallel-strategy-development --setup --profiles "conservative,aggressive,custom"

# Specific strategy focus
@parallel-strategy-development --setup --focus "market_making,momentum"

# Tournament with custom criteria  
@parallel-strategy-development --tournament --min-score 85 --max-strategies 2

# Auto-merge after tournament
@parallel-strategy-development --tournament --auto-merge
```

#### Worktree Management
```bash
# List all strategy worktrees
git worktree list | grep hft-strategies

# Cleanup failed/completed worktrees
@parallel-strategy-development --cleanup --remove-failed

# Synchronize infrastructure updates across all worktrees
@parallel-strategy-development --sync-infrastructure

# Broadcast urgent fixes to all active worktrees
@parallel-strategy-development --broadcast-update "critical infrastructure fix"
```

#### Development Workflow Integration
```bash
# Morning development routine
@parallel-strategy-development --setup          # Setup fresh worktrees
# ... agents develop throughout day ...
@parallel-strategy-development --status         # Afternoon progress check
@parallel-strategy-development --tournament     # Evening evaluation
@parallel-strategy-development --merge          # Integration of winners
```

### Performance Considerations

#### Disk Space Management
```bash
# Each worktree requires ~500MB for full repository copy
# 4 worktrees = ~2GB additional disk usage
# Monitor with: du -sh ../hft-strategies/*
```

#### Build Performance
```bash
# Each worktree has independent build directory
# Parallel compilation across worktrees possible
# Consider: make -j$(nproc) in each worktree for faster builds
```

#### Memory Usage
```bash
# Multiple Claude Code sessions increase memory usage
# 4 concurrent sessions = ~8-12GB RAM usage
# Monitor system resources during development
```

### Troubleshooting

#### Common Issues

**Worktree Creation Fails**:
```bash
# Check for uncommitted changes in main repository
git status
git stash  # if needed

# Verify git worktree support
git worktree --help
```

**Agent Session Conflicts**:
```bash
# Each agent should work in its own worktree directory
# Verify current directory before launching Claude Code
pwd  # Should show worktree path, not main repository
```

**Build Failures**:
```bash
# Clean and reconfigure build in problematic worktree
cd ../hft-strategies/hft-problematic
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cd build && make -j
```

**Merge Conflicts**:
```bash
# Manual resolution required for complex conflicts
git status
# Edit conflicted files
git add .
git commit -m "Resolve merge conflicts for strategy integration"
```

### Best Practices

#### Agent Coordination
1. **Clear objectives** - Each agent has distinct, non-overlapping goals
2. **Independent validation** - Agents test strategies in isolation
3. **Performance consistency** - All strategies must meet HFT performance requirements
4. **Risk compliance** - All strategies integrate with existing risk management

#### Development Discipline
1. **Comprehensive testing** - Unit tests, benchmarks, integration tests required
2. **Performance validation** - Strategy decision latency <1μs enforced
3. **Integration compliance** - Object pools, ring buffers, timing framework mandatory
4. **Documentation standards** - Clear strategy documentation and parameter explanation

#### Tournament Fairness
1. **Standardized metrics** - All strategies evaluated using same criteria
2. **Risk adjustment** - Performance scores account for risk characteristics
3. **Multiple dimensions** - Evaluation includes returns, risk, consistency, robustness
4. **Objective selection** - Automated scoring prevents human bias

This parallel development framework enables systematic strategy development with complete agent isolation, fair competition, and clean integration of successful approaches into the main HFT trading system.

## Command Development Standards

### Implementation Requirements
- **Complete functionality** - Commands must handle all edge cases and error conditions
- **User feedback** - Clear progress indication and informative error messages
- **Integration compliance** - Must work with existing HFT infrastructure and patterns
- **Performance consideration** - Commands should not introduce unnecessary latency
- **Documentation completeness** - Comprehensive usage examples and troubleshooting guides

### Testing and Validation
- **Functionality testing** - All command paths tested with realistic scenarios
- **Error handling** - Graceful handling of failures and edge cases
- **Performance impact** - Commands must not degrade system performance
- **Integration testing** - Validation with existing development workflow
- **User experience** - Commands should improve development productivity

This commands directory provides the workflow automation necessary for efficient HFT development with Claude Code, enabling complex multi-agent development scenarios while maintaining the high performance and reliability standards required for financial trading systems.
