#!/bin/bash
# format.sh - Format code with clang-format
# Usage: format.sh [--check]

set -e

CHECK_ONLY=false

if [ "$1" = "--check" ]; then
    CHECK_ONLY=true
fi

# Find all C++ source files
FILES=$(find src tests include \( -name '*.cpp' -o -name '*.hpp' \))

if [ "$CHECK_ONLY" = true ]; then
    echo "Checking code formatting..."
    echo "$FILES" | xargs clang-format --dry-run --Werror
    echo "✓ Format check passed"
else
    echo "Formatting code..."
    echo "$FILES" | xargs clang-format -i
    echo "✓ Code formatted"
fi
