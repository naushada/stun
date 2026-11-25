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

## Layout

```
stun
├── modules/stun/
│   ├── core/          # shared message format (encode/decode, XOR-MAPPED-ADDRESS)
│   │   ├── inc/stun.h
│   │   ├── src/stun.cpp
│   │   └── test/core_test.cpp
│   ├── client/        # Binding request + CLI
│   │   ├── inc/stun_client.h
│   │   ├── src/stun_client.cpp
│   │   ├── src/main.cpp
│   │   └── test/client_test.cpp
│   └── server/        # Binding response + CLI
│       ├── inc/stun_server.h
│       ├── src/stun_server.cpp
│       ├── src/main.cpp
│       └── test/server_test.cpp
├── scripts/
│   ├── build.sh       # podman build
│   └── run.sh         # podman run (client or server)
├── Dockerfile
└── CMakeLists.txt
```

## Building (Podman only)

```bash
./scripts/build.sh
```

Or manually:

```bash
podman build -t stun:latest .
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
`stun-client-test`, `stun-server-test` test binaries.

## License

MIT — see [LICENSE](LICENSE).
