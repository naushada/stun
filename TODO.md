# TODO for STUN Implementation (RFC 8489)

This file documents ongoing development priorities and future enhancements for the STUN implementation.

## High Priority Features

### Phase 1: Core Functionality

- [ ] **MD5 Hash Integration**
  - Replace simplified MD5 with OpenSSL integration
  - Implement proper HMAC-SHA256 for message integrity
  - Add unit tests for cryptographic operations

- [ ] **Candidate Ordering Algorithm**
  - Implement RFC 8489 Section 6 ordering algorithm
  - Support for dynamic candidate reordering
  - Track relative preference changes

- [ ] **Protocol ACK (Prack) Support**
  - Implement Prack Request/Response handling
  - Reliability protocol integration
  - Add support for multiple Prack instances

### Phase 2: Enhancement

- [ ] **TURN Integration**
  - Connect with TURN server for relayed candidates
  - Implement fallback mechanism when relay unavailable
  - Support for candidate signaling in TURN

- [ ] **RFC 8489 Full Compliance**
  - Add support for RFC 8489 Section 5.3 (Response Processing)
  - Handle MAPPING TYPE variations
  - Improve error handling and validation

### Phase 3: Advanced Features

- [ ] **IPv6 Support Enhancement**
  - Full IPv6 address parsing with compression
  - Test against IPv6 STUN servers
  - Dual-stack candidate generation

- [ ] **Performance Optimization**
  - Parallel processing for multiple candidates
  - Cache-based response retrieval
  - Memory-efficient data structures

### Phase 4: Security

- [ ] **Message Integrity Verification**
  - Implement proper MD5 verification (not simplified)
  - Add transport security layers
  - Handle potential message tampering detection

- [ ] **Input Validation**
  - Validate all incoming STUN attributes
  - Reject malformed MAPPED-RESPONSE values
  - Buffer overflow protection

## Low Priority Items

### Documentation

- [ ] Update README with new API examples
- [ ] Add detailed RFC 8489 reference implementation notes
- [ ] Create comprehensive API documentation (Doxygen style)
- [ ] Write unit test documentation for each function

### Testing

- [ ] Increase coverage to >90% for all parsing functions
- [ ] Add stress testing for high-load scenarios
- [ ] Test against actual STUN server endpoints (not just mocks)

### Maintenance

- [ ] Update README.md with latest build instructions
- [ ] Refresh STUN server list (remove deprecated servers)
- [ ] Document dependency changes

## Development Schedule

| Phase | Duration | Status | Notes |
|-------|----------|--------|-------|
| Phase 1 | ~2 weeks | Pending | Core RFC compliance |
| Phase 2 | ~3 weeks | Pending | Enhancements and integration |
| Phase 3 | ~4 weeks | Pending | Advanced features |
| Phase 4 | Ongoing | N/A | Security maintenance |

## Known Issues

### Performance Issues

- MD5 computation using simplified version (not production-ready)
- Need to integrate proper OpenSSL library for cryptographic operations
- Candidate ordering algorithm needs optimization for multiple server responses

### RFC Compliance Gaps

- Full Prack Request support not yet implemented
- Message integrity verification uses simplified hashing
- Some edge cases in candidate generation not handled

## Contributors

- Robert Greiner - Initial high-performance parser design (RFC 8489)
- RFC 8489 Protocol Specification authors

### Development Team

Add team members as they contribute:
