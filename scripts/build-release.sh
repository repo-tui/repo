#!/bin/bash
# build-release.sh - Build optimized static binary for release
# Usage: build-release.sh [build_dir]

set -e

BUILD_DIR="${1:-build}"
NUM_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "Building release binary..."
echo "  Build directory: $BUILD_DIR"
echo "  Parallel jobs: $NUM_CORES"

# Configure with CMake (Release mode, static linking where possible)
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DREPO_BUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# Build
cmake --build "$BUILD_DIR" --config Release -j "$NUM_CORES"

# Strip debug symbols
if [ -f "$BUILD_DIR/repo" ]; then
    echo "Stripping debug symbols..."
    strip "$BUILD_DIR/repo"

    # Show binary size
    BINARY_SIZE=$(du -h "$BUILD_DIR/repo" | cut -f1)
    echo "✓ Binary built: $BUILD_DIR/repo ($BINARY_SIZE)"
else
    echo "Error: Binary not found at $BUILD_DIR/repo" >&2
    exit 1
fi
