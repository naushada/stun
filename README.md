# STUN

A minimal [STUN](https://datatracker.ietf.org/doc/html/rfc8489) (Session
Traversal Utilities for NAT) implementation with a client, a server, and an
appliance log-shipping setup.

The client sends a Binding request to a STUN server and reports the public
(reflexive) address the server sees. The server answers Binding requests with
the client's reflexive address in a `XOR-MAPPED-ADDRESS` attribute.

## Features

- Correct 20-byte STUN message header (type, length, magic cookie, 96-bit
  transaction id) with big-endian encoding and 4-byte attribute padding.
- `XOR-MAPPED-ADDRESS` and `MAPPED-ADDRESS` encoding/decoding for IPv4 and IPv6.
- Client (Binding request) and server (Binding response) with no external
  dependencies.
- Built-in leveled logging library with syslog semantics (see [Logging](#logging)).
- Appliance log shipping via chrony + syslog-ng (see [Log shipping](#log-shipping-appliance)).

## Layout

```
stun
├── logging/              # leveled logging library (static lib stun_log)
│   ├── inc/log.h
│   ├── src/log.cpp
│   ├── test/log_test.cpp
│   └── CMakeLists.txt
├── modules/stun/
│   ├── core/             # shared message format (encode/decode, XOR-MAPPED-ADDRESS)
│   │   ├── inc/stun.h
│   │   ├── src/stun.cpp
│   │   ├── test/core_test.cpp
│   │   └── CMakeLists.txt
│   ├── client/           # Binding request + CLI
│   │   ├── inc/stun_client.h
│   │   ├── src/stun_client.cpp
│   │   ├── src/main.cpp
│   │   ├── test/client_test.cpp
│   │   └── CMakeLists.txt
│   └── server/           # Binding response + CLI
│       ├── inc/stun_server.h
│       ├── src/stun_server.cpp
│       ├── src/main.cpp
│       ├── test/server_test.cpp
│       └── CMakeLists.txt
├── thirdparty/           # appliance log shipping (chrony + syslog-ng)
│   ├── collect.sh
│   ├── chrony/chrony.conf
│   └── syslog-ng/
├── scripts/
│   ├── build.sh          # podman build
│   └── run.sh            # podman run (client or server)
├── Dockerfile
└── CMakeLists.txt
```

## Logging

`logging/` builds a static library (`stun_log`) used by the client and server.

- **Levels** mirror syslog severities: `Emergency`, `Alert`, `Critical`, `Error`,
  `Warning`, `Notice`, `Info`, `Debug`. The default threshold is `Notice`.
- **Per-module thresholds**: `logging::set_level("client", logging::Level::Debug)`.
- **Two backends**:
  - `Stderr` — single-line JSON (default), ideal for containers.
  - `Syslog` — traditional `syslog(3)`, captured by syslog-ng on the appliance.
- The app selects its backend from `STUN_LOG_BACKEND` (`stderr` default, or
  `syslog` / `all`) via `logging::init_from_env()`.

```cpp
logging::init(logging::Backend::Stderr);   // or init_from_env()
logging::set_level("server", logging::Level::Debug);
STUN_LOG_NOTICE("client", "querying %s", host);
```

## Log shipping (appliance)

`thirdparty/collect.sh` runs `chronyd` (accurate time) and `syslog-ng` (log
collection/forwarding), then the STUN app. syslog-ng pushes to a remote endpoint
only when `LOG_REMOTE_HOST` is set; otherwise it keeps logs local.

```bash
# Local only ("does nothing" remote):
./thirdparty/collect.sh

# Forward to a remote syslog server:
LOG_REMOTE_HOST=logs.example.com LOG_REMOTE_PORT=6514 ./thirdparty/collect.sh

# Run the server instead of the client:
STUN_APP=server ./thirdparty/collect.sh
```

See [thirdparty/README.md](thirdparty/README.md) for the full set of variables
and how to point it at Elasticsearch/cloud instead of a syslog endpoint.

## Building (Podman only)

```bash
./scripts/build.sh
```

Or manually:

```bash
podman build -f Dockerfile -t stun:latest .
```

## Running

```bash
# Query a STUN server (default: stun.l.google.com:19302)
./scripts/run.sh client

# Query a specific server
./scripts/run.sh client stun.l.google.com:19302

# Run the local STUN server on udp/3478
./scripts/run.sh server
```

The client prints the mapped address as `host:port`.

## Building and testing natively (CMake)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

Targets: `stun-client`, `stun-server`, and the `stun-core-test`,
`stun-client-test`, `stun-server-test`, `stun-log-test` test binaries.

## License

MIT — see [LICENSE](LICENSE).
