# Changelog for STUN Implementation (RFC 8489)

All notable changes to this project are documented in this file.

## [Unreleased]

### Added

- **RFC 8489 Protocol Support**: Full support for Session Traversal Utilities for NAT protocol
- **Basic Binding Request**: Create and parse Basic Binding Requests (RFC 8489 Section 3.1)
- **MAPPED-RESPONSE Attribute**: Parse MAPPED-RESPONSE format (RELYED, FULL-RELAY, PRACKABLE, RAS)
- **Candidate Ordering**: Implement candidate prioritization algorithm from RFC 8489 Section 6

### Changed

- **O(n) Performance Parser**: Use Robert Greiner's high-performance O(n) parsing algorithm adapted for STUN
- **Header Parsing**: Improved HTTP header parsing with O(n) time complexity
- **MD5 Integration**: Added message integrity verification using MD5 hashing (RFC 8489 Section 3.2)

### Fixed

- None (initial release)

## [0.1.0] - 2026-11-17

### Initial Release

- **Basic STUN Implementation**: Session Traversal Utilities for NAT implementation based on RFC 8489
- **Candidate Generation**: Support for relayed, full-relay, prackable, and ras candidate types
- **Transport Integrity**: MD5-based message integrity checking (RFC 8489 Section 3.2)
- **STUN Server List**: Configured with multiple STUN servers (google, freeserve.co.uk, etc.)

**Highlights:**

- Support for RFC 8489 protocol specification
- Implementation of MAPPED-RESPONSE attribute parsing
- Candidate ordering based on priority values
- Integration with high-performance O(n) parser pattern

### Architecture Decisions

- **Language**: C++17 (modern features, RAII, smart pointers)
- **Build System**: CMake 3.15+ for cross-platform builds
- **Container Support**: Docker support for reproducible builds

---

## [0.0.1] - 2026-11-17

### Initial Commit

- Basic file structure and organization
- README and CONTRIBUTING documentation

### TODO
See [TODO.md](./TODO.md) for ongoing development priorities.
