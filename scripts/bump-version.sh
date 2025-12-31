#!/bin/bash
# bump-version.sh - Bump semantic version based on branch prefix
# Usage: bump-version.sh <prefix> <current_version>
# Returns new version to stdout

set -e

PREFIX=$1
CURRENT=$2

if [ -z "$PREFIX" ] || [ -z "$CURRENT" ]; then
    echo "Usage: bump-version.sh <prefix> <current_version>" >&2
    echo "  prefix: epic/, feat/, fix/, or docs/" >&2
    echo "  current_version: X.Y.Z format" >&2
    exit 1
fi

# Validate version format
if ! [[ $CURRENT =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Invalid version format '$CURRENT'. Expected X.Y.Z" >&2
    exit 1
fi

# Parse current version
IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT"

# Bump based on prefix
case "$PREFIX" in
    epic/*)
        NEW_VERSION="$((MAJOR+1)).0.0"
        ;;
    feat/*)
        NEW_VERSION="$MAJOR.$((MINOR+1)).0"
        ;;
    fix/*)
        NEW_VERSION="$MAJOR.$MINOR.$((PATCH+1))"
        ;;
    docs/*)
        # No version bump for documentation changes
        NEW_VERSION="$CURRENT"
        ;;
    *)
        echo "Error: Unknown prefix '$PREFIX'. Expected epic/, feat/, fix/, or docs/" >&2
        exit 1
        ;;
esac

echo "$NEW_VERSION"
