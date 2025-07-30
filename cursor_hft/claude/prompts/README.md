# Claude Code Prompts Directory

This directory contains reusable prompt templates for common HFT development scenarios. These templates provide consistent starting points for specific development tasks and can be customized for particular situations.

## Prompts vs Commands: Key Differences

### **Commands** (`claude/commands/`)
**What they are**: Complete workflows that orchestrate multiple steps and agent coordination  
**Purpose**: Automation of complex development processes  
**Scope**: Multi-step processes, agent coordination, system-wide workflows  
**Usage**: Execute complete workflows (setup, coordination, analysis)  

**Examples**:
- `parallel-strategy-development` - Sets up git worktrees, coordinates multiple agents, runs tournaments
- `parallel-code-review` - Orchestrates 4 different agents for comprehensive reviews
- `prime-project-context` - Loads multiple docs, analyzes structure, generates comprehensive context

### **Prompts** (`claude/prompts/`)
**What they are**: Template starting points for specific development tasks  
**Purpose**: Consistent, reusable prompts for common scenarios  
**Scope**: Single-focused development tasks with customizable parameters  
**Usage**: Quick start templates that you customize for specific situations  

**Examples**:
- `strategy_development.md` - Template prompts for different strategy types
- `component_implementation.md` - Templates for infrastructure component development
- `performance_optimization.md` - Templates for different optimization scenarios

### **When to Use Which**

**Use Commands when**:
- You need complex multi-step workflows
- Multiple agents need coordination
- System-wide setup or analysis is required
- You want full automation of a process

**Use Prompts when**:
- You need a starting point for a specific task
- You want to customize the approach for your situation
- You're working on a focused development problem
- You need consistent language across similar tasks

### **Example Comparison**

**Command Example**: `@parallel-strategy-development --setup`
- Creates git worktrees
- Sets up build environments  
- Configures specialized agent prompts
- Coordinates 4 agents with different profiles
- Manages the entire parallel development process

**Prompt Example**: Using `strategy_development.md` template
- You copy a template (e.g., "Conservative Market Making Strategy")
- Customize it for your specific needs (instrument, risk tolerance, etc.)
- Use with any agent: `@hft-strategy-developer` + customized prompt
- Get focused development on your specific strategy requirement

## Available Prompt Templates

### `strategy_development.md` - Trading Strategy Templates
**Purpose**: Consistent prompts for different trading strategy development scenarios  
**Use Cases**: Strategy development, algorithm design, trading logic implementation  

**Template Categories**:
- Conservative Market Making
- Aggressive Momentum Trading
- Statistical Arbitrage
- Adaptive Learning Strategies
- Risk Management Integration
- Performance Optimization

### `component_implementation.md` - Infrastructure Component Templates
**Purpose**: Standard prompts for building high-performance infrastructure components  
**Use Cases**: Component development, performance optimization, integration tasks  

**Template Categories**:
- Memory Management Components
- Messaging and Communication
- Data Structures and Algorithms  
- Timing and Measurement
- ARM64 Optimization
- Lock-Free Implementations

### `performance_optimization.md` - Performance Analysis Templates
**Purpose**: Systematic prompts for different performance optimization scenarios  
**Use Cases**: Bottleneck analysis, regression investigation, optimization planning  

**Template Categories**:
- Latency Analysis and Optimization
- Throughput Improvement
- Memory Access Pattern Optimization
- ARM64-Specific Optimization
- Regression Investigation
- System-Wide Performance Analysis

### `code_review.md` - Code Review Templates
**Purpose**: Structured prompts for different types of code reviews  
**Use Cases**: Quality assurance, performance validation, integration compliance  

**Template Categories**:
- Performance-Focused Reviews
- Risk and Safety Reviews
- Architecture and Integration Reviews
- HFT Domain-Specific Reviews
- Pre-Production Reviews

## How to Use Prompt Templates

### Basic Usage
1. **Choose appropriate template** based on your development task
2. **Copy the relevant section** from the template file
3. **Customize parameters** for your specific situation
4. **Use with appropriate agent** (`@hft-strategy-developer`, `@hft-component-builder`, etc.)

### Example Workflow
```bash
# 1. Load agent
@claude/agents/hft-strategy-developer.md

# 2. Use customized prompt from template
"Using the Conservative Market Making template from claude/prompts/strategy_development.md:

Develop a conservative market making strategy for 10Y Treasury Notes with the following customizations:
- Target Sharpe ratio: >2.5 (higher than template default)
- Maximum position: $50M (specify size)
- Market hours: 9:00 AM - 3:00 PM ET (Treasury cash market)
- Inventory target: Flat by end of day

[Rest of template prompt...]"
```

### Customization Guidelines
- **Replace placeholder values** with your specific requirements
- **Adjust performance targets** based on your component/strategy needs
- **Modify risk parameters** to match your risk tolerance
- **Specify integration requirements** for your particular use case

## Template Structure

Each prompt template follows this structure:

### Header Section
- **Template name and purpose**
- **Target agent recommendation**
- **Use case description**
- **Customization parameters**

### Core Prompt
- **Objective statement**
- **Technical requirements**
- **Performance targets**
- **Integration requirements**

### Customization Points
- **[PARAMETER]** - Values you should replace
- **[OPTIONAL]** - Additional requirements you can add
- **[CHOICE_A|CHOICE_B]** - Alternative approaches to choose from

### Validation Criteria
- **Success metrics**
- **Performance validation**
- **Integration compliance**
- **Testing requirements**

## Integration with Development Workflow

### Daily Development
```bash
# Quick strategy development
@hft-strategy-developer + strategy_development.md template

# Component optimization  
@hft-component-builder + component_implementation.md template

# Performance investigation
@hft-performance-optimizer + performance_optimization.md template
```

### Code Review Process
```bash
# Focused review preparation
Copy relevant template from code_review.md
Customize for specific review type (performance/safety/architecture)
Use with @parallel-code-review command for comprehensive analysis
```

### Team Consistency
- **Standardized language** across team members
- **Consistent requirements** for similar tasks
- **Reproducible development** approaches
- **Quality baselines** for all development work

## Template Maintenance

### When to Update Templates
- **Performance targets change** (update baseline requirements)
- **New integration patterns** emerge (add to templates)
- **Common customizations** identified (create new template variants)
- **Team feedback** suggests improvements

### How to Extend Templates
1. **Identify common customization patterns**
2. **Create new template variants**
3. **Update documentation** with new use cases
4. **Test templates** with actual development scenarios

### Template Quality Standards
- **Specific and actionable** prompts
- **HFT performance requirements** clearly stated
- **Integration compliance** explicitly required
- **Customization guidance** provided
- **Success criteria** clearly defined

## Best Practices

### Template Selection
- **Match template to task type** (strategy, component, optimization, review)
- **Consider agent specialization** (use appropriate agent with template)
- **Evaluate complexity level** (simple templates for focused tasks)

### Customization
- **Replace all placeholder values** with specific requirements
- **Add context-specific details** relevant to your situation
- **Specify performance targets** appropriate for your component
- **Include integration requirements** for your specific use case

### Quality Assurance
- **Validate template usage** produces expected results
- **Ensure consistency** with established patterns
- **Verify performance compliance** with HFT requirements
- **Test integration** with existing infrastructure

## Prompt Template Benefits

### Consistency
- **Standardized development approaches** across the team
- **Consistent quality expectations** for all work
- **Reproducible development processes** 
- **Common language and terminology**

### Efficiency
- **Faster development startup** with proven templates
- **Reduced cognitive load** for common tasks
- **Less time spent crafting prompts** from scratch
- **Focus on customization** rather than prompt creation

### Quality
- **Proven prompt patterns** that work well with agents
- **HFT-specific requirements** built into templates
- **Performance targets** clearly specified
- **Integration compliance** automatically included

### Learning
- **Example prompts** for new team members
- **Best practice demonstrations** in template structure
- **Customization examples** showing flexibility
- **Success pattern documentation**

This prompt template system provides the consistency and efficiency needed for high-quality HFT development while maintaining the flexibility to customize approaches for specific situations and requirements.
