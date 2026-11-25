# Third-party log shipping (appliance)

The appliance uses two off-the-shelf daemons, spawned by `collect.sh`:

- **`chronyd`** — NTP client for accurate timestamps (so log timestamps are
  trustworthy when correlated across hosts).
- **`syslog-ng`** — collects local logs and forwards them to a remote endpoint
  when one is configured; otherwise it keeps them local only ("does nothing"
  remote).

## How it works

The STUN app logs to syslog (`STUN_LOG_BACKEND=syslog`, which `collect.sh`
exports). syslog-ng's `system()` source reads those messages from `/dev/log`
and routes them per its configuration.

`collect.sh` (the container entrypoint / appliance supervisor):

1. Starts `chronyd` if it is installed.
2. Starts `syslog-ng` with either a remote or a local-only configuration.
3. Runs the STUN app.

## Configuration (environment variables)

| Variable             | Default  | Description |
|----------------------|----------|-------------|
| `LOG_REMOTE_HOST`    | (unset)  | Remote syslog host. Unset ⇒ local-only (nothing is pushed). |
| `LOG_REMOTE_PORT`    | `6514`   | Remote syslog port. |
| `LOG_REMOTE_TRANSPORT` | `tcp`  | `tcp` or `tls`. For `tls`, add a `tls(ca-file(...))` block in `syslog-ng/syslog-ng.conf.tmpl`. |
| `STUN_APP`           | `client` | `client` or `server`. |
| `STUN_LOG_BACKEND`   | `syslog` | Backend the app uses (`stderr`, `syslog`, `all`). |

## Example

```bash
# Local only (collect, but do not push anywhere):
./thirdparty/collect.sh

# Forward to a remote syslog server:
LOG_REMOTE_HOST=logs.example.com LOG_REMOTE_PORT=6514 ./thirdparty/collect.sh

# Run the server instead of the client:
STUN_APP=server ./thirdparty/collect.sh
```

For Elasticsearch/cloud rather than a syslog endpoint, replace the
`destination d_remote { syslog(...); };` block in
`syslog-ng/syslog-ng.conf.tmpl` with syslog-ng's `elasticsearch-http()` or
`http()` destination.
