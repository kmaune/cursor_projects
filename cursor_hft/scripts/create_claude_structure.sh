#!/bin/bash
# create_claude_structure.sh - Create Claude Code file structure for GitHub

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT"

echo "Creating Claude Code structure for version control..."

# Create main claude directory structure
mkdir -p claude/{agents,commands,prompts,context}

# Create agent files (empty for now)
touch claude/agents/hft-code-reviewer.md
touch claude/agents/hft-strategy-developer.md
touch claude/agents/hft-component-builder.md
touch claude/agents/hft-performance-optimizer.md

# Create command files (empty for now)
touch claude/commands/prime-project-context.md
touch claude/commands/parallel-code-review.md
touch claude/commands/parallel-strategy-development.md
touch claude/commands/quick-setup.md

# Create context files (empty for now)
touch claude/context/current_status.md
touch claude/context/integration_patterns.md
touch claude/context/performance_targets.md
touch claude/context/development_standards.md

# Create prompt files for common scenarios
touch claude/prompts/strategy_development.md
touch claude/prompts/component_implementation.md
touch claude/prompts/performance_optimization.md
touch claude/prompts/code_review.md

# Create README for the claude directory
cat > claude/README.md << 'EOF'
# Claude Code Configuration

This directory contains Claude Code agents, commands, and context files optimized for HFT trading system development.

## Structure

- `agents/` - Project-specific Claude Code agents
- `commands/` - Development workflow commands
- `context/` - Project context and reference materials
- `prompts/` - Reusable prompt templates

## Usage

1. **Prime Claude Code**: Use `prime-project-context` command to load full project context
2. **Code Review**: Use `parallel-code-review` for comprehensive multi-agent reviews
3. **Strategy Development**: Use `parallel-strategy-development` for multi-agent strategy creation
4. **Component Building**: Use HFT-specific agents for infrastructure components

## Agent Specializations

- `hft-code-reviewer` - HFT-specific code review with performance focus
- `hft-strategy-developer` - Trading strategy implementation
- `hft-component-builder` - Infrastructure component development
- `hft-performance-optimizer` - Performance analysis and optimization

See individual files for detailed agent configurations and usage instructions.
EOF

# Create .gitignore additions for claude directory
cat >> .gitignore << 'EOF'

# Claude Code local configurations (if any)
.claude/
claude/.local/
EOF

echo "✅ Claude Code structure created!"
echo ""
echo "📁 Directory structure:"
tree claude/
echo ""
echo "📝 Next steps:"
echo "1. Fill in agent configurations in claude/agents/"
echo "2. Add command definitions in claude/commands/"
echo "3. Document context in claude/context/"
echo "4. Commit to git: git add claude/ && git commit -m 'Add Claude Code configuration'"
