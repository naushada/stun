#!/usr/bin/env bash
# Build the STUN container image with Podman.
set -euo pipefail
cd "$(dirname "$0")/.."

podman build -f Dockerfile -t stun:latest .
