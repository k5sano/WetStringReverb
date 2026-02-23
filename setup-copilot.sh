#!/bin/bash
set -e

echo "=== WetStringReverb Copilot Setup ==="

# Create directories
mkdir -p .github/agents
mkdir -p .github/skills/handoff-reader
mkdir -p .github/skills/juce-impl
mkdir -p .github/skills/juce-testing
mkdir -p .github/skills/build-check
mkdir -p .github/skills/juce-builder
mkdir -p .github/hooks
mkdir -p .copilot/handoff
mkdir -p scaffold
mkdir -p scripts
mkdir -p Source/DSP
mkdir -p Tests

# Create .gitkeep files
touch .copilot/handoff/.gitkeep
touch scaffold/.gitkeep

echo "=== Directory structure created ==="
echo ""
echo "Next steps:"
echo "  1. Place JUCE framework in ./JUCE (git submodule recommended)"
echo "  2. Run: cmake -B build -DCMAKE_BUILD_TYPE=Debug"
echo "  3. Run: cmake --build build/"
echo "  4. Run: ctest --test-dir build/"
echo ""
echo "Or use Claude Code:"
echo "  claude --agent juce-builder"
echo ""
echo "Done. Run 'git add -A && git commit' to record."
