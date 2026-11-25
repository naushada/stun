#!/usr/bin/env bash
# Run the STUN client (default) or server inside the container.
set -euo pipefail
cd "$(dirname "$0")/.."

mode="${1:-client}"
case "$mode" in
  client)
    exec podman run --rm stun:latest ./build/stun-client "${@:2}"
    ;;
  server)
    exec podman run --rm -p 3478:3478/udp stun:latest ./build/stun-server "${@:2}"
    ;;
  *)
    echo "usage: $0 [client|server] [args...]" >&2
    exit 1
    ;;
esac
