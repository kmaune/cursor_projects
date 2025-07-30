---
name: parallel-strategy-development
description: Orchestrate parallel strategy development using multiple Claude Code agents with git worktree isolation. Creates independent development environments for different trading profiles, enables tournament-style strategy competition, and provides merge workflow for successful strategies.
---

Launch parallel strategy development using multiple specialized Claude Code agents, each working in isolated git worktrees to prevent conflicts and enable independent iteration toward optimal trading strategies.

## Parallel Strategy Development Framework

### Phase 1: Worktree Environment Setup

**Create isolated development environments for each agent profile:**

```bash
# Worktree Creation Strategy
STRATEGY_PROFILES=("conservative" "aggressive" "quantitative" "adaptive")
BASE_DIR="../hft-strategies"

# Create specialized worktrees for each agent
for profile in "${STRATEGY_PROFILES[@]}"; do
    WORKTREE_DIR="${BASE_DIR}/hft-${profile}"
    BRANCH_NAME="strategy/${profile}-$(date +%Y%m%d)"
    
    echo "Creating worktree for ${profile} agent..."
    git worktree add "${WORKTREE_DIR}" -b "${BRANCH_NAME}"
    
    # Setup build environment in each worktree
    cd "${WORKTREE_DIR}"
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cd - > /dev/null
done

echo "✅ Created isolated worktrees for parallel development"
```

### Phase 2: Agent Profile Configuration

**Define specialized agent profiles with distinct trading objectives:**

#### Agent 1: Conservative Market Maker
**Worktree**: `../hft-strategies/hft-conservative`
**Branch**: `strategy/conservative-YYYYMMDD`
**Profile Configuration**:
```bash
AGENT_PROFILE="conservative"
RISK_TOLERANCE="low"
OPTIMIZATION_TARGET="sharpe_ratio"
MAX_DRAWDOWN="2%"
POSITION_HOLDING_TIME="<10s"
STRATEGY_FOCUS="market_making"

# Specialized prompt for conservative agent
CONSERVATIVE_PROMPT="Develop a conservative market making strategy for Treasury markets optimizing for risk-adjusted returns.

OBJECTIVES:
- Minimize drawdown and volatility (max 2% drawdown)
- Maximize Sharpe ratio (target >2.0)
- Conservative position sizing with quick exits
- Sophisticated spread calculation with inventory adjustment
- Enhanced risk management with circuit breakers

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Risk calculations: <200ns
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Risk-first approach to strategy design
- Robust error handling and edge case management
- Comprehensive backtesting and scenario analysis
- Conservative parameter selection with safety margins"
```

#### Agent 2: Aggressive Momentum Trader
**Worktree**: `../hft-strategies/hft-aggressive`
**Branch**: `strategy/aggressive-YYYYMMDD`
**Profile Configuration**:
```bash
AGENT_PROFILE="aggressive"
RISK_TOLERANCE="high" 
OPTIMIZATION_TARGET="absolute_returns"
MAX_POSITION="50%"
HOLDING_TIME="1-30s"
STRATEGY_FOCUS="momentum"

# Specialized prompt for aggressive agent
AGGRESSIVE_PROMPT="Develop an aggressive momentum strategy for Treasury markets optimizing for maximum absolute returns.

OBJECTIVES:
- Maximize profit per trade (target >0.01% per trade)
- Accept higher volatility for return opportunities
- Larger position sizes when signal confidence is high
- Trend-following and breakout capture algorithms
- Dynamic position sizing based on momentum strength

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Signal processing: <300ns
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Return-first approach with calculated risk taking
- Advanced momentum indicators and signal processing
- Dynamic position sizing and risk scaling
- Sophisticated trend detection algorithms"
```

#### Agent 3: Quantitative Statistical Arbitrageur
**Worktree**: `../hft-strategies/hft-quantitative`
**Branch**: `strategy/quantitative-YYYYMMDD`
**Profile Configuration**:
```bash
AGENT_PROFILE="quantitative"
RISK_TOLERANCE="medium"
OPTIMIZATION_TARGET="win_rate"
TARGET_WIN_RATE="70%"
HOLDING_TIME="<5s"
STRATEGY_FOCUS="statistical_arbitrage"

# Specialized prompt for quantitative agent
QUANTITATIVE_PROMPT="Develop a statistical arbitrage strategy based on quantitative models for Treasury markets.

OBJECTIVES:
- Exploit statistical inefficiencies and mean reversion patterns
- Target high win rate (>70%) with consistent small profits
- Model-based signal generation with rigorous backtesting
- Statistical relationship analysis and correlation trading
- Systematic approach to market inefficiency detection

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Model calculations: <400ns
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Quantitative model development and validation
- Statistical significance testing for all signals
- Robust parameter estimation and model selection
- Systematic backtesting with walk-forward analysis"
```

#### Agent 4: Adaptive Learning Agent
**Worktree**: `../hft-strategies/hft-adaptive`
**Branch**: `strategy/adaptive-YYYYMMDD`
**Profile Configuration**:
```bash
AGENT_PROFILE="adaptive"
RISK_TOLERANCE="dynamic"
OPTIMIZATION_TARGET="continuous_improvement"
LEARNING_FREQUENCY="real_time"
STRATEGY_FOCUS="machine_learning"

# Specialized prompt for adaptive agent
ADAPTIVE_PROMPT="Develop an adaptive strategy that learns from Treasury market behavior and adjusts parameters in real-time.

OBJECTIVES:
- Dynamic parameter adjustment based on market conditions
- Online learning from market behavior and strategy performance
- Multi-timeframe signal integration and regime detection
- Continuous improvement metrics and performance optimization
- Self-adjusting risk management based on market volatility

PERFORMANCE REQUIREMENTS:
- Strategy decision latency: <1μs (target <500ns)
- Learning updates: <200ns
- Memory allocation: Zero in hot paths
- Integration: Object pools, ring buffers, timing framework

IMPLEMENTATION FOCUS:
- Online learning algorithms suitable for real-time trading
- Adaptive parameter optimization with market regime detection
- Performance feedback loops and continuous improvement
- Dynamic risk adjustment based on learned market patterns"
```

### Phase 3: Parallel Agent Execution

**Launch Claude Code agents in parallel worktrees:**

```bash
# Agent Execution Framework
execute_parallel_agents() {
    local profiles=("conservative" "aggressive" "quantitative" "adaptive")
    
    for profile in "${profiles[@]}"; do
        WORKTREE_DIR="../hft-strategies/hft-${profile}"
        
        echo "🤖 Launching ${profile} agent in ${WORKTREE_DIR}"
        
        # Create agent execution script for each worktree
        cat > "${WORKTREE_DIR}/run_agent.sh" << EOF
#!/bin/bash
cd "${WORKTREE_DIR}"

# Load project context in this worktree
@claude/commands/prime-project-context.md

# Activate strategy development agent with profile-specific prompt
@claude/agents/hft-strategy-developer.md

# Execute profile-specific development prompt
echo "Agent Profile: ${profile}"
echo "Worktree: ${WORKTREE_DIR}"
echo "Branch: \$(git branch --show-current)"
echo ""
echo "Development Prompt:"
echo "\${${profile^^}_PROMPT}"

# Start interactive development session
echo "Ready for ${profile} strategy development..."
EOF
        
        chmod +x "${WORKTREE_DIR}/run_agent.sh"
        
        # Optionally auto-launch Claude Code sessions
        # (Manual launch recommended for control)
        echo "  → Run: cd ${WORKTREE_DIR} && ./run_agent.sh"
    done
    
    echo ""
    echo "🚀 All agent environments prepared!"
    echo "Launch Claude Code in each worktree directory to begin parallel development."
}
```

### Phase 4: Strategy Development Coordination

**Coordinate independent strategy development:**

```bash
# Development Status Monitoring
monitor_strategy_progress() {
    echo "📊 Strategy Development Status"
    echo "=============================="
    
    for profile in "conservative" "aggressive" "quantitative" "adaptive"; do
        WORKTREE_DIR="../hft-strategies/hft-${profile}"
        
        if [ -d "${WORKTREE_DIR}" ]; then
            cd "${WORKTREE_DIR}"
            
            echo ""
            echo "🤖 ${profile^} Agent Status:"
            echo "  ├── Worktree: ${WORKTREE_DIR}"
            echo "  ├── Branch: $(git branch --show-current)"
            echo "  ├── Commits: $(git rev-list --count HEAD ^main)"
            echo "  ├── Files changed: $(git diff --name-only main | wc -l)"
            
            # Check for strategy implementation
            if [ -f "include/hft/strategy/${profile}_strategy.hpp" ]; then
                echo "  ├── Strategy implementation: ✅ Present"
            else
                echo "  ├── Strategy implementation: ⚠️  Not found"
            fi
            
            # Check for tests
            if [ -f "tests/strategy/${profile}_strategy_test.cpp" ]; then
                echo "  ├── Unit tests: ✅ Present"
            else
                echo "  ├── Unit tests: ⚠️  Not found"
            fi
            
            # Check for benchmarks
            if [ -f "benchmarks/strategy/${profile}_strategy_benchmark.cpp" ]; then
                echo "  ├── Benchmarks: ✅ Present"
            else
                echo "  ├── Benchmarks: ⚠️  Not found"
            fi
            
            # Build status
            if [ -d "build" ]; then
                cd build
                if make -q > /dev/null 2>&1; then
                    echo "  └── Build status: ✅ Passing"
                else
                    echo "  └── Build status: ❌ Failing"
                fi
                cd ..
            else
                echo "  └── Build status: ⚠️  Not configured"
            fi
            
            cd - > /dev/null
        else
            echo "🤖 ${profile^} Agent: ❌ Worktree not found"
        fi
    done
}
```

### Phase 5: Tournament Framework

**Implement strategy competition and evaluation:**

```bash
# Strategy Tournament System
run_strategy_tournament() {
    echo "🏆 Strategy Tournament Framework"
    echo "==============================="
    
    TOURNAMENT_DIR="./tournament_results_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "${TOURNAMENT_DIR}"
    
    echo "Tournament directory: ${TOURNAMENT_DIR}"
    echo ""
    
    # Collect all implemented strategies
    local strategies=()
    for profile in "conservative" "aggressive" "quantitative" "adaptive"; do
        WORKTREE_DIR="../hft-strategies/hft-${profile}"
        STRATEGY_FILE="${WORKTREE_DIR}/include/hft/strategy/${profile}_strategy.hpp"
        
        if [ -f "${STRATEGY_FILE}" ]; then
            strategies+=("${profile}")
            echo "✅ Found ${profile} strategy implementation"
            
            # Copy strategy to tournament directory for comparison
            mkdir -p "${TOURNAMENT_DIR}/${profile}"
            cp "${STRATEGY_FILE}" "${TOURNAMENT_DIR}/${profile}/"
            
            # Copy tests and benchmarks if they exist
            [ -f "${WORKTREE_DIR}/tests/strategy/${profile}_strategy_test.cpp" ] && \
                cp "${WORKTREE_DIR}/tests/strategy/${profile}_strategy_test.cpp" "${TOURNAMENT_DIR}/${profile}/"
            
            [ -f "${WORKTREE_DIR}/benchmarks/strategy/${profile}_strategy_benchmark.cpp" ] && \
                cp "${WORKTREE_DIR}/benchmarks/strategy/${profile}_strategy_benchmark.cpp" "${TOURNAMENT_DIR}/${profile}/"
        else
            echo "⚠️  ${profile} strategy not ready for tournament"
        fi
    done
    
    if [ ${#strategies[@]} -eq 0 ]; then
        echo "❌ No strategies ready for tournament"
        return 1
    fi
    
    echo ""
    echo "🏁 Tournament Participants: ${strategies[*]}"
    echo "📊 Running comparative analysis..."
    
    # Tournament Analysis Framework
    cat > "${TOURNAMENT_DIR}/run_tournament.cpp" << 'EOF'
#include "hft/strategy/strategy_tournament.hpp"

int main() {
    hft::strategy::StrategyTournament tournament;
    
    // Register all available strategies
    // (Implementation would load each strategy dynamically)
    
    // Run tournament with historical data
    auto results = tournament.run_comprehensive_tournament();
    
    // Generate tournament report
    tournament.generate_tournament_report(results);
    
    return 0;
}
EOF
    
    echo "📋 Tournament framework created in ${TOURNAMENT_DIR}"
    echo "💡 Next steps:"
    echo "   1. Implement StrategyTournament class"
    echo "   2. Run comparative benchmarks"
    echo "   3. Analyze performance metrics"
    echo "   4. Select winning strategies for merge"
}
```

### Phase 6: Strategy Selection and Merge

**Select and integrate best performing strategies:**

```bash
# Strategy Selection and Merge Workflow
merge_winning_strategies() {
    echo "🏆 Strategy Selection and Merge"
    echo "=============================="
    
    # Strategy performance analysis (placeholder for actual tournament results)
    declare -A STRATEGY_SCORES=(
        ["conservative"]="85"    # Risk-adjusted score
        ["aggressive"]="78"      # Return-based score  
        ["quantitative"]="92"    # Consistency score
        ["adaptive"]="88"        # Improvement score
    )
    
    # Selection criteria (configurable)
    local MIN_SCORE=80
    local MAX_STRATEGIES=3
    
    echo "📊 Strategy Performance Scores:"
    for strategy in "${!STRATEGY_SCORES[@]}"; do
        score=${STRATEGY_SCORES[$strategy]}
        if [ "$score" -ge "$MIN_SCORE" ]; then
            echo "  ✅ ${strategy}: ${score}/100 (PASS)"
        else
            echo "  ❌ ${strategy}: ${score}/100 (FAIL)"
        fi
    done
    
    echo ""
    echo "🎯 Selecting strategies for merge (score >= ${MIN_SCORE}):"
    
    # Sort strategies by score and select top performers
    local winning_strategies=()
    for strategy in $(printf '%s\n' "${!STRATEGY_SCORES[@]}" | \
                     while read strategy; do
                         echo "${STRATEGY_SCORES[$strategy]} $strategy"
                     done | sort -nr | head -n "$MAX_STRATEGIES" | \
                     awk -v min="$MIN_SCORE" '$1 >= min {print $2}'); do
        winning_strategies+=("$strategy")
    done
    
    if [ ${#winning_strategies[@]} -eq 0 ]; then
        echo "❌ No strategies meet minimum performance criteria"
        return 1
    fi
    
    echo "🏆 Selected strategies: ${winning_strategies[*]}"
    echo ""
    
    # Merge winning strategies
    echo "🔄 Merging winning strategies to main branch:"
    for strategy in "${winning_strategies[@]}"; do
        WORKTREE_DIR="../hft-strategies/hft-${strategy}"
        BRANCH_NAME=$(cd "$WORKTREE_DIR" && git branch --show-current)
        
        echo "  📥 Merging ${strategy} strategy..."
        echo "      Branch: ${BRANCH_NAME}"
        echo "      Worktree: ${WORKTREE_DIR}"
        
        # Merge strategy branch to main
        git merge --no-ff "$BRANCH_NAME" -m "Merge ${strategy} strategy from parallel development
        
Performance Score: ${STRATEGY_SCORES[$strategy]}/100
Agent Profile: ${strategy}
Development Branch: ${BRANCH_NAME}
Tournament Results: Passed selection criteria"
        
        if [ $? -eq 0 ]; then
            echo "      ✅ Merge successful"
        else
            echo "      ❌ Merge failed - manual resolution required"
        fi
    done
    
    echo ""
    echo "🎉 Strategy integration complete!"
    echo "📊 New strategies available in main branch"
}
```

## Command Usage Examples

### Basic Parallel Development
```bash
# Setup and launch parallel strategy development
@parallel-strategy-development --setup

# Monitor progress
@parallel-strategy-development --status

# Run tournament when strategies are ready
@parallel-strategy-development --tournament

# Merge winning strategies
@parallel-strategy-development --merge
```

### Advanced Configuration
```bash
# Custom agent profiles
@parallel-strategy-development --setup --profiles "conservative,aggressive,custom"

# Specific strategy focus
@parallel-strategy-development --setup --focus "market_making,momentum"

# Tournament with custom criteria
@parallel-strategy-development --tournament --min-score 85 --max-strategies 2
```

### Development Workflow Integration
```bash
# Morning: Setup parallel development
@parallel-strategy-development --setup

# Afternoon: Check progress
@parallel-strategy-development --status --detailed

# Evening: Run tournament and merge winners
@parallel-strategy-development --tournament --auto-merge
```

## Worktree Management Commands

### Cleanup and Maintenance
```bash
# List all strategy worktrees
git worktree list | grep hft-strategies

# Remove completed/failed worktrees
@parallel-strategy-development --cleanup --remove-failed

# Synchronize infrastructure updates across worktrees
@parallel-strategy-development --sync-infrastructure
```

### Development Coordination
```bash
# Share common updates across all strategy worktrees
@parallel-strategy-development --broadcast-update "infrastructure fix"

# Collect tournament data from all worktrees
@parallel-strategy-development --collect-results --output tournament_summary.json
```

## Expected Workflow

1. **Setup**: `@parallel-strategy-development --setup` creates isolated worktrees
2. **Development**: Launch Claude Code in each worktree with specialized agents
3. **Monitoring**: `@parallel-strategy-development --status` tracks progress
4. **Tournament**: `@parallel-strategy-development --tournament` compares strategies
5. **Integration**: `@parallel-strategy-development --merge` integrates winners
6. **Cleanup**: Remove unsuccessful strategy worktrees

This framework enables true parallel strategy development with complete isolation, systematic comparison, and clean integration of successful strategies into the main codebase.
