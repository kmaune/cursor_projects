# HFT Trading System - Claude Code Agents

This directory contains specialized Claude Code agents optimized for high-frequency trading system development. Each agent is designed with specific expertise and focus areas to provide the most effective assistance for different aspects of HFT development.

## How Claude Code Agents Work

### Agent Activation
Claude Code agents are activated by including their configuration in your project. When you reference an agent, Claude Code loads their specialized knowledge, tools, and behavioral patterns.

### Usage Patterns
```bash
# In Claude Code interface, reference specific agents:
@hft-code-reviewer     # Activate the HFT code reviewer
@hft-strategy-developer # Activate the strategy development specialist

# Or in prompts/conversations:
"Using the hft-code-reviewer agent, please review this order book implementation..."
"I need the hft-strategy-developer to help create a momentum trading strategy..."
```

### Agent Configuration
Each agent is defined in a markdown file with:
- **name**: Unique identifier for the agent
- **description**: What the agent does and when to use it
- **tools**: What capabilities the agent has access to
- **specialized knowledge**: Domain expertise and focus areas
- **response patterns**: How the agent structures its output

## Available HFT Agents

### 1. `hft-code-reviewer` 
**Purpose**: Comprehensive code review with HFT-specific expertise  
**Best For**: 
- Code quality and correctness validation
- Performance regression detection  
- HFT trading logic verification
- Integration pattern compliance
- Risk management validation

**When to Use**:
- Before merging any trading system code
- When implementing new components
- For performance-critical code changes
- When adding new trading strategies
- During architecture reviews

**Expertise Areas**:
- Sub-microsecond performance analysis
- Memory allocation detection (zero allocation enforcement)
- Cache efficiency and ARM64 optimization
- Trading logic correctness and risk assessment
- Integration with existing HFT infrastructure

### 2. `hft-strategy-developer`
**Purpose**: Trading strategy design and implementation  
**Best For**:
- Creating new trading strategies
- Optimizing strategy performance (<2μs decision latency)
- Risk management integration
- Multi-agent strategy coordination
- Strategy backtesting and validation

**When to Use**:
- Developing new market making strategies
- Creating momentum or mean reversion algorithms
- Implementing statistical arbitrage strategies
- Building adaptive/ML-enhanced strategies
- Optimizing existing strategy performance

**Expertise Areas**:
- Quantitative trading strategy design
- Market microstructure and order flow analysis
- Ultra-low latency optimization
- Risk management and position sizing
- Strategy performance measurement and optimization

### 3. `hft-component-builder`
**Purpose**: Infrastructure component development  
**Best For**:
- Building high-performance system components
- Cache-optimized data structure design
- Zero-allocation memory management
- Lock-free algorithm implementation
- ARM64-specific optimization

**When to Use**:
- Creating new infrastructure components
- Optimizing existing component performance
- Implementing lock-free data structures
- Building custom memory managers
- Developing ARM64-optimized algorithms

**Expertise Areas**:
- Sub-microsecond component design
- Cache-friendly memory layouts
- Lock-free and wait-free algorithms
- Template-based performance optimization
- Hardware-specific optimization (ARM64)

### 4. `hft-performance-optimizer`
**Purpose**: Performance analysis and optimization  
**Best For**:
- System-wide performance analysis
- Bottleneck identification and resolution
- Latency budget optimization
- Regression detection and prevention
- Benchmark analysis and interpretation

**When to Use**:
- When performance regressions are detected
- For system-wide optimization initiatives
- During performance validation phases
- When analyzing benchmark results
- For capacity planning and scaling analysis

**Expertise Areas**:
- Profiling and performance analysis
- ARM64 architecture optimization
- Latency budget management
- Memory hierarchy optimization
- System-wide performance coordination

## Agent Usage Guidelines

### 1. **Choose the Right Agent**
- **Code Quality Issues** → `hft-code-reviewer`
- **New Strategy Development** → `hft-strategy-developer`  
- **Infrastructure Components** → `hft-component-builder`
- **Performance Problems** → `hft-performance-optimizer`

### 2. **Agent Coordination**
Agents can work together for complex tasks:
```
Complex Strategy Implementation:
1. hft-strategy-developer → Design and implement strategy
2. hft-component-builder → Optimize supporting infrastructure  
3. hft-code-reviewer → Review complete implementation
4. hft-performance-optimizer → Validate and optimize performance
```

### 3. **Context Integration**
All agents understand the current HFT system:
- Current performance achievements (667ns tick-to-trade)
- Proven architectural patterns (object pools, ring buffers)
- Integration requirements (treasury data, ARM64 optimization)
- Testing and validation standards

### 4. **Expected Outputs**

**hft-code-reviewer**:
- Categorized issues by severity (Critical/Major/Minor)
- Specific fix recommendations with HFT context
- Performance impact assessment
- Integration compliance validation

**hft-strategy-developer**:
- Complete strategy implementations with tests
- Performance benchmarks validating <2μs decision latency
- Risk management integration
- Documentation of algorithm design and optimizations

**hft-component-builder**:
- Single-header component implementations
- Comprehensive test suites (unit + performance + integration)
- CMakeLists.txt integration
- Performance validation against targets

**hft-performance-optimizer**:
- Detailed performance analysis with bottleneck identification
- Specific optimization recommendations with impact estimates
- Benchmark interpretation and regression analysis
- System-wide optimization roadmaps

## Best Practices

### 1. **Be Specific About Context**
```bash
# Good: Specific context and expectations
"Using hft-code-reviewer, review this market making strategy implementation. 
Focus on the position management logic and ensure it integrates correctly 
with our object pool patterns."

# Less Good: Vague request
"Review this code"
```

### 2. **Leverage Agent Expertise**
Each agent has deep domain knowledge - let them apply their specialized expertise rather than giving overly prescriptive instructions.

### 3. **Sequential Agent Usage**
For complex tasks, use agents in logical sequence:
1. **Design** (strategy-developer or component-builder)
2. **Review** (code-reviewer)  
3. **Optimize** (performance-optimizer)

### 4. **Integration with Project Standards**
All agents understand and enforce:
- Zero allocation in hot paths
- Sub-microsecond performance requirements
- Object pool and ring buffer integration
- ARM64 cache alignment (64-byte)
- Comprehensive testing requirements

## Agent Development and Customization

### Adding New Agents
To add specialized agents for your specific needs:

1. **Create agent file**: `claude/agents/new-agent-name.md`
2. **Define expertise**: Specify domain knowledge and focus areas
3. **Set behavioral patterns**: Define how the agent should respond
4. **Update this README**: Document the new agent's purpose and usage

### Modifying Existing Agents
Agents can be customized by:
- Adjusting their expertise areas
- Modifying response patterns
- Adding new tools or capabilities
- Updating performance targets or requirements

## Integration with Development Workflow

### Daily Development
- **Pre-commit**: Use `hft-code-reviewer` for all changes
- **New Features**: Start with appropriate specialist agent
- **Performance Issues**: Immediately engage `hft-performance-optimizer`

### Major Development Phases
- **Phase 4 Strategy Development**: Primary use of `hft-strategy-developer`
- **Infrastructure Expansion**: Focus on `hft-component-builder`
- **Performance Optimization**: Coordinate multiple agents for system-wide improvements

### Quality Assurance
All agents enforce the established HFT standards:
- Performance targets (validated sub-microsecond operation)
- Integration patterns (object pools, ring buffers, timing framework)
- Testing requirements (functional + performance + integration)
- Documentation standards (comprehensive inline explanations)

This agent system ensures consistent, high-quality HFT development while leveraging specialized expertise for different aspects of the trading system.
