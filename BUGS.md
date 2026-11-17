# Known Issues (STUN Implementation)

This document describes known issues with the STUN implementation based on RFC 8489.

## Current Issues

### Critical Issues

#### 1. MD5 Hash Implementation - Not Production Ready

**Severity**: High  
**Status**: To Be Fixed  

**Description**:
- The current implementation uses a simplified MD5 computation (not actual OpenSSL MD5)
- Message integrity verification is incomplete and may not detect all tampering attacks

**Impact**: 
- Security vulnerabilities in message integrity checking
- Potential man-in-the-middle attacks possible

**Planned Fix**:
```cpp
// Replace with proper OpenSSL integration
#include <openssl/evp.h>

std::vector<uint8_t> compute_md5(const std::vector<uint8_t>& data) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit(ctx, EVP_md_size(EVP_md_type(EVP_digest_info)));
    EVP_DigestUpdate(ctx, data.data(), data.size());
    unsigned char hash[EVP_max_digest_size(EVP_md_type(EVP_digest_info))];
    unsigned int digest_len = 0;
    EVP_DigestFinal_ex(ctx, hash, &digest_len);
    std::vector<uint8_t> result(hash, hash + digest_len);
    EVP_MD_CTX_free(ctx);
    return result;
}
```

**RFC Reference**: RFC 8489 Section 3.2 (Message Integrity Check)

---

#### 2. IPv6 Address Parsing - Simplified Format Support

**Severity**: Medium  
**Status**: Known Limitation  

**Description**:
- Only basic IPv4 addresses fully supported
- Partial IPv6 support with compressed format handling

**Impact**:
- May not correctly parse all IPv6-stun.com responses

**Workaround**: Use full-qualified IPv6 addresses

---

### Low Priority Issues

#### 3. Limited Test Coverage

**Severity**: Low  
**Status**: Planned Enhancement  

**Description**:
- Current test coverage <90% (simplified implementation)
- Edge case handling for various STUN attribute combinations not comprehensive

**Planned Fix**:
```cpp
// Increase coverage to >90%
TEST(StunMessage, MAPPEDRESPONSE_With_Complex_Attributes) {
    // Full test matrix of attribute combinations
}
```

#### 4. Missing Prack Request Support

**Severity**: Low  
**Status**: RFC Compliance Gap  

**Description**:
- Protocol ACK (Prack) support not fully implemented
- Some STUN server implementations may require Prack requests

**RFC Reference**: RFC 8489 Section 3.1, Section 5.2

---

## Future Issues to Watch

### Performance Optimization

**Severity**: Low  
**Status**: Continuous Improvement  

**Description**:
- Current implementation not optimized for high-throughput scenarios
- Memory usage may increase with large candidate lists

**Planned Fix**:
```cpp
// Parallel processing for multiple candidates
std::async(StunMessage(msg1), StunMessage(msg2));
```

### Security Updates

**Severity**: Medium  
**Status**: Ongoing Maintenance  

**Description**:
- Regular updates needed as STUN protocol evolves
- New cryptographic standards may replace current MD5

---

## RFC 8489 Compliance Gaps

| Feature | Status | Notes |
|---------|--------|-------|
| Basic Binding Request | ✅ Implemented | Works per RFC 8489 Section 3.1 |
| MAPPED-RESPONSE Parsing | ⚠️ Partial | Simplified implementation |
| Candidate Ordering | ✅ Implemented | Follows priority algorithm |
| Prack Requests | ❌ Not Yet | Required by some STUN servers |
| Message Integrity | ⚠️ Simplified | Need OpenSSL integration |

---

## Recommendations

1. **Short-term**: Implement proper MD5/SHA256 hash verification using OpenSSL
2. **Medium-term**: Add full Prack Request support for protocol compliance
3. **Long-term**: Optimize performance for high-throughput scenarios

**Note**: This implementation is based on Robert Greiner's high-performance O(n) HTTP parser, adapted for STUN requirements from RFC 8489 protocol specification.
