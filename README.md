# STUN

A minimal, dependency-free [STUN](https://datatracker.ietf.org/doc/html/rfc8489)
(Session Traversal Utilities for NAT) implementation with a client and a server.

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
  - `Syslog` — traditional `syslog(3)` with a configurable facility.
- **Cloud / Elasticsearch**: the JSON backend emits one line per event, e.g.
  `{"timestamp":"...","level":"notice","module":"client","message":"..."}`.
  Tail it with Fluentd/Filebeat/Vector (or a container log driver) and ship it
  straight to Elasticsearch or a cloud log store — no ES client is baked into
  the library.

```cpp
logging::init();                          // defaults: Stderr backend + Notice level
logging::set_level("server", logging::Level::Debug);
STUN_LOG_NOTICE("client", "querying %s", host);
```

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
