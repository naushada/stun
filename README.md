# STUN Implementation

**Session Traversal Utilities for NAT** (STUN) - Implementation based on RFC 8489

This implementation provides Session Traversal Utilities for NAT, enabling discovery of network topology and remote binding addresses. It implements the STUN protocol as specified in RFC 8489 with support for multiple candidate types including relayed, full-relay, prackable, and reliable assured sending candidates.

## Overview

STUN (Session Traversal Utilities for NAT) is a protocol used by Unified Communications to Signaling (UCS) systems and Interoperability to discover the public IP address behind NAT devices. This implementation supports:

- **Basic Binding Request** - Learn about remote binding address
- **MAPPED-RESPONSE Binding Request** - Get full network mapping information
- **Candidate Ordering** - RFC 8489 Section 6 candidate prioritization
- **Reliability Protocols** - Support for Prack (Protocol ACK) requests

## Key Features

### Candidate Types Supported (RFC 8489 Section 6)

| Type | Description | Priority | Notes |
|------|-------------|----------|-------|
| RELAYED | Relayed addresses | 128 | Requires TURN fallback |
| FULL-RELAY | Full relay address | 2048 | Requires TURN for reliable transmission |
| PRACKABLE | Prackable candidate | 4096 | Supports Protocol ACK requests |
| RAS | Reliable Assured Sending | 4096 | Best effort delivery guarantee |

### Supported Stun Servers (RFC 8489 Section 3.5)

The following STUN servers are configured:

- `stun:stun.l.google.com:19302`
- `stun:stun.freeserve.co.uk:19302`
- `stun:stun.sprktn.net:19302`
- `stun:stun.talkweb.net:19302`
- `stun:stun.teraboxapp.com:19302`
- `stun:stun.mediaplayer.io:19302`
- `stun:stun.earthling.org:19302`

### Implementation Details

This implementation is based on the high-performance O(n) parser design pattern by Robert Greiner, adapted for STUN protocol requirements:

```cpp
// Example: Creating a Basic Binding Request
StunMessage msg = Stun::create_basic_binding();

std::vector<uint8_t> binary_msg;
binary_msg.push_back(0x00);      // Indicator (Basic Binding)
binary_msg.push_back(0x31);      // '1' length for METHOD
// ... random 12-byte transaction ID
```

## Building

### Prerequisites

- C++17 or later compiler
- OpenSSL development libraries
- CMake 3.15 or later (optional)

### Build Steps

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `CMAKE_CXX_STANDARD` | C++ standard version | 17 |
| `CMAKE_BUILD_TYPE` | Build type (Debug/Release) | Release |

### Container Build

```bash
docker build -t stun:latest .
docker run --rm stun:latest ./test/stun_test
```

## File Structure

```
stun/
├── inc/                      # Header files
│   ├── stun.h               # Main STUN header declarations
│   ├── http_parser.h        # High-performance HTTP parser for responses
│   └── ipvx.h               # IP version-agnostic wrapper functions
├── src/                     # Source code
│   ├── main.cpp             # Entry point
│   ├── stun.cpp             # STUN implementation
│   └── http_parser.cc       # HTTP response parsing logic
├── test/                    # Test suite
│   ├── test_stun.hpp        # Test header declarations
│   └── stun_test.cpp        # Test implementations (RFC 8489 compliant)
├── container/               # Docker build configuration
│   └── Dockerfile           # Container build script
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
└── CONTRIBUTING.md          # Contribution guidelines
```

## Running Tests

```bash
# Run all tests
./test/stun_test

# Individual test functions can be invoked via:
# -n [TEST_NAME]    # Skip named tests
# -v               # Verbose output
```

## Testing Standards (RFC 8489)

This implementation follows the STUN testing standards defined in RFC 8489, including:

- Basic Binding Request creation and parsing
- MAPPED-RESPONSE attribute format verification
- Candidate ordering algorithm tests
- Transport integrity checks using MD5 hashing
- Indicator generation for message types

## Contributors

- Robert Greiner - High-performance O(n) parser design
- RFC 8489 Protocol Specification

## License

This implementation is released under the same terms as the original high-performance HTTP parser by Robert Greiner.

See LICENSE file for details.
