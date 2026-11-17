# AGENTS: STUN Implementation Agents (RFC 8489)

This document describes the agent roles and responsibilities for the STUN implementation project.

## Agent Roles

### Primary Agent: ParserImpl

**Role**: High-performance O(n) HTTP response parser  
**Responsible**: Parse MAPPED-RESPONSE attributes with zero-time complexity guarantee

```cpp
class ParserImpl {
public:
    bool parse_http_header(const std::string& input, 
                          std::map<std::string, std::string>& headers);
private:
    enum class State : uint8_t {
        PRE_START_REQ_OR_RES,
        HEADER_FIELD_START,
        HEADER_VALUE,
        HEADERS_DONE,
        ...
    };
};
```

**Implementation**: `/src/http_parser.cc` - Line 428 handles header map population

---

### Secondary Agent: Utility::compute_md5

**Role**: Message integrity verification  
**Responsible**: MD5 hash computation per RFC 8489 Section 3.2

```cpp
namespace Utility {
    std::vector<uint8_t> compute_md5(const std::vector<uint8_t>& data);
}
```

**Implementation**: `/src/stun.cpp` - Message construction with cryptographic integrity

---

### Tertiary Agent: NetworkDiscovery::get_stun_servers

**Role**: STUN server list management  
**Responsible**: Iterate over RFC 8489 Section 3.5 configured servers

```cpp
namespace NetworkDiscovery {
    const std::vector<std::string>& get_stun_servers();
}
```

**Implementation**: `/src/stun.cpp` - Server enumeration with fallback strategies

---

## Agent Communication Protocol

### Message Format

```cpp
enum class AgentMessageType {
    INDICATOR = 0,      // Basic message indicator
    METHOD = 1,         // Method attribute (BING)
    RELATION = 2,       // Relation attribute (PRACK)
    MAPPED = 3          // Mapping information
};
```

---

## Agent Hierarchy

```
ParserImpl (O(n) Parser)
    └─> Utility::compute_md5 (Integrity Check)
        └─> NetworkDiscovery::get_stun_servers (Server Discovery)
            └─> MessageConstruction::create_basic_binding (Request Builder)
                └─> BindingRequest (Protocol ACK Support)
```

---

## Agent Status

| Agent | Status | Last Update | Notes |
|-------|--------|--------------|-------|
| ParserImpl | ✅ Active | RFC 8489 Section 3.2 | O(n) parser active |
| Utility::compute_md5 | ⚠️ Simplified | Pending OpenSSL update | Hash verification simplified |
| NetworkDiscovery | ✅ Active | RFC 8489 Section 3.5 | Server list current |

---

## Agent Requirements

### ParserImpl
- Must maintain O(n) time complexity (RFC 8489 requirement)
- Header parsing must be linear time
- Zero-memory allocations during header processing

### Utility::compute_md5
- Must implement proper MD5 hashing
- Return 16-byte hash value per RFC 8489
- Compatible with OpenSSL libraries

---

## Agent Coordination Protocol

```cpp
// Example: ParserImpl coordinating with agents
void ParserImpl::parse_http_header(const std::string& input,
                                    std::map<std::string, std::string>& headers) {
    // State machine transitions for O(n) parsing
    while (line_start < input.length()) {
        header_field_mark = line_start;
        
        if (char_found == ':') {
            headers[key] = value;  // Map insertion
            
            // Coordinate with MessageConstruction::create_basic_binding()
            return;
        }
    }
}
```

---

**Reference**: RFC 8489 Section 3.2 (Message Integrity Check)  
**Maintainer**: Robert Greiner - High-performance O(n) Parser pattern  
