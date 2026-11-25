# STUN

A minimal, dependency-free [STUN](https://datatracker.ietf.org/doc/html/rfc8489)
(Session Traversal Utilities for NAT) client library and command-line tool.

It sends a Binding request to a STUN server and reports the public
(reflexive) address the server sees, which is useful for NAT discovery.

## Features

- Correct 20-byte STUN message header (type, length, magic cookie, 96-bit
  transaction id) with big-endian encoding and 4-byte attribute padding.
- Binding request generation with a random transaction id.
- `XOR-MAPPED-ADDRESS` and `MAPPED-ADDRESS` encoding/decoding for IPv4 and IPv6.
- Single-header library (`inc/stun.h`) with no external dependencies.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces three targets:

| Target        | Description                                  |
|---------------|----------------------------------------------|
| `stun`        | Static library (the STUN client)             |
| `stun-client` | CLI that queries a STUN server               |
| `stun-test`   | Self-contained unit tests (no test framework)|

### Container build

```bash
podman build -t stun -f container/Dockerfile .
podman run --rm stun
```

## Usage

```bash
# Defaults to stun.l.google.com:19302
./build/stun-client

# Query a specific server
./build/stun-client stun.l.google.com:19302
```

On success the tool prints the mapped address as `host:port`.

## Tests

```bash
ctest --test-dir build
# or directly:
./build/stun-test
```

## Layout

```
stun/
├── inc/
│   └── stun.h        # Library header
├── src/
│   ├── stun.cpp      # Library implementation
│   └── main.cpp      # CLI entry point
├── test/
│   └── stun_test.cpp # Unit tests
├── container/
│   └── Dockerfile    # Container build
└── CMakeLists.txt
```

## License

MIT — see [LICENSE](LICENSE).
