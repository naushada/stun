#!/usr/bin/env bash
# Appliance supervisor: start chronyd + syslog-ng, then run the STUN app.
#
# Logs are collected by syslog-ng and pushed to a remote endpoint only when
# LOG_REMOTE_HOST is set; otherwise they are kept local ("does nothing" remote).
#
# Env:
#   LOG_REMOTE_HOST       remote syslog host (unset => local-only)
#   LOG_REMOTE_PORT       remote port (default 6514)
#   LOG_REMOTE_TRANSPORT  "tcp" or "tls" (default tcp)
#   STUN_APP              "client" or "server" (default client)
#   STUN_LOG_BACKEND      app log backend (default syslog)
set -euo pipefail

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SELF_DIR/.." && pwd)"

# 1. Accurate time (NTP), if chronyd is installed.
if command -v chronyd >/dev/null 2>&1 && ! pgrep -x chronyd >/dev/null 2>&1; then
    echo "starting chronyd" >&2
    chronyd -f "$SELF_DIR/chrony/chrony.conf"
fi

# 2. Build the syslog-ng config: remote if configured, else local-only.
CONF="$SELF_DIR/syslog-ng/syslog-ng-local.conf"
if [ -n "${LOG_REMOTE_HOST:-}" ]; then
    CONF="$(mktemp /tmp/syslog-ng-remote.XXXXXX)"
    sed -e "s|@REMOTE_HOST@|${LOG_REMOTE_HOST}|g" \
        -e "s|@REMOTE_PORT@|${LOG_REMOTE_PORT:-6514}|g" \
        -e "s|@REMOTE_TRANSPORT@|${LOG_REMOTE_TRANSPORT:-tcp}|g" \
        "$SELF_DIR/syslog-ng/syslog-ng.conf.tmpl" > "$CONF"
fi

# 3. Start syslog-ng (foreground, backgrounded here) and wait for /dev/log.
if command -v syslog-ng >/dev/null 2>&1; then
    echo "starting syslog-ng" >&2
    syslog-ng -f "$CONF" --no-caps --foreground &
    for _ in $(seq 1 50); do
        [ -S /dev/log ] && break
        sleep 0.1
    done
fi

# 4. Run the STUN app with syslog logging so syslog-ng collects it.
export STUN_LOG_BACKEND="${STUN_LOG_BACKEND:-syslog}"
case "${STUN_APP:-client}" in
    client) exec "$ROOT/build/stun-client" "$@" ;;
    server) exec "$ROOT/build/stun-server" "$@" ;;
    *) echo "unknown STUN_APP: ${STUN_APP}" >&2; exit 1 ;;
esac
