# Claude Code Context Directory

This directory is **currently empty by design** - we've chosen to leverage the excellent existing project documentation instead of duplicating content.

## Current Approach: Use Existing Documentation

The `prime-project-context` command intelligently reads from existing files:
- `Claude.md` - Primary development context and proven patterns
- `README.md` - Project overview and achievements
- `hft_project_plan.md` - Detailed project progress
- `docs/ARCHITECTURE.md` - System design
- `docs/PERFORMANCE_REPORT.md` - Performance analysis
- `docs/AUTOMATED_BENCHMARKS.md` - Benchmark system

This avoids duplication and leverages your already-excellent documentation.

## Future Use Cases (If Needed)

If you later decide to add agent-specific context files, this directory could contain:

### `current_status.md` - Project Status Summary
**Purpose**: Quick status reference for agents
**Content**: Distilled project phase, performance achievements, readiness assessment
**When to create**: If agents need faster status lookup than reading full Claude.md

### `integration_patterns.md` - Code Pattern Reference
**Purpose**: Quick code pattern lookup for agents
**Content**: Standard integration patterns, mandatory code templates, examples
**When to create**: If agents frequently need pattern references during code generation

### `performance_targets.md` - Performance Requirements
**Purpose**: Quick performance target lookup for agents
**Content**: Component-specific latency targets, throughput requirements, validation criteria
**When to create**: If agents need frequent performance target validation

### `development_standards.md` - Coding Standards
**Purpose**: Quick coding standard reference for agents
**Content**: Code style requirements, testing patterns, build integration standards
**When to create**: If agents need detailed style guidance beyond existing docs

### `strategy_profiles.md` - Strategy Development Profiles
**Purpose**: Agent profile definitions for parallel strategy development
**Content**: Conservative/Aggressive/Quantitative/Adaptive agent profiles and parameters
**When to create**: When implementing Phase 4 parallel strategy development

### `optimization_playbooks.md` - Performance Optimization Guides
**Purpose**: Systematic optimization approaches for different scenarios
**Content**: Step-by-step optimization procedures, ARM64-specific techniques, measurement protocols
**When to create**: When agents need structured optimization workflows

## Decision Criteria for Adding Context Files

**Add context files when:**
- Agents frequently need the same information that requires parsing large documents
- You want agent-specific formatting (more code examples, less narrative)
- You need information that doesn't exist in current docs
- You want faster agent loading times

**Don't add context files when:**
- Information exists in current documentation
- The overhead of maintaining duplicate content isn't justified
- Current `prime-project-context` command provides sufficient context

## Current Recommendation: Keep Empty

Your existing documentation is comprehensive and well-structured. The `prime-project-context` command effectively leverages this documentation to provide complete agent context without duplication.

**Only add context files if you encounter specific agent performance or accuracy issues that would be resolved by agent-specific documentation formats.**
