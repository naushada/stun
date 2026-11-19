#!/bin/bash
# STUN Container Build Script

set -e

BUILD_DIR="/Users/naushada/repo/stun/build"
SOURCE_DIR=".."

echo "Building STUN container image..."

podman build -t stun-image \
  -f $SOURCE_DIR/container/Dockerfile \
  $SOURCE_DIR

echo "Container build complete!"
