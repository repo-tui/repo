#!/bin/bash
# package-tarball.sh - Create release tarball
# Usage: package-tarball.sh <version> <arch> [build_dir]

set -e

VERSION=$1
ARCH=$2
BUILD_DIR="${3:-build}"

if [ -z "$VERSION" ] || [ -z "$ARCH" ]; then
    echo "Usage: package-tarball.sh <version> <arch> [build_dir]" >&2
    echo "  version: e.g., 0.2.0" >&2
    echo "  arch: e.g., linux-x86_64, darwin-aarch64" >&2
    exit 1
fi

PACKAGE_NAME="repo-v$VERSION"
TARBALL_NAME="$PACKAGE_NAME-$ARCH.tar.gz"
TEMP_DIR="/tmp/$PACKAGE_NAME-$ARCH"

echo "Creating release tarball..."
echo "  Version: $VERSION"
echo "  Architecture: $ARCH"
echo "  Tarball: $TARBALL_NAME"

# Clean up previous temp directory
rm -rf "$TEMP_DIR"

# Create package structure
mkdir -p "$TEMP_DIR/bin"

# Copy binary
if [ ! -f "$BUILD_DIR/repo" ]; then
    echo "Error: Binary not found at $BUILD_DIR/repo" >&2
    echo "Run build-release.sh first" >&2
    exit 1
fi

cp "$BUILD_DIR/repo" "$TEMP_DIR/bin/"

# Copy documentation
for file in LICENSE README.md CHANGELOG.md; do
    if [ -f "$file" ]; then
        cp "$file" "$TEMP_DIR/"
    fi
done

# Create tarball
tar czf "$TARBALL_NAME" -C "/tmp" "$PACKAGE_NAME-$ARCH"

# Clean up
rm -rf "$TEMP_DIR"

# Show result
TARBALL_SIZE=$(du -h "$TARBALL_NAME" | cut -f1)
echo "✓ Created: $TARBALL_NAME ($TARBALL_SIZE)"
