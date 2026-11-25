// stun.h — shared STUN (RFC 8489) message format: encode/decode and
// XOR-MAPPED-ADDRESS codec. Used by both the client and the server.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace stun {

// STUN message types (RFC 8489 Section 6).
constexpr uint16_t kBindingRequest  = 0x0001;
constexpr uint16_t kBindingResponse = 0x0101;
constexpr uint16_t kBindingError    = 0x0111;

// The magic cookie (RFC 8489 Section 6), also used to XOR-encode addresses.
constexpr uint32_t kMagicCookie = 0x2112A442;

// Attribute types (RFC 8489 Sections 14 and 18).
constexpr uint16_t kAttrMappedAddress    = 0x0001;
constexpr uint16_t kAttrXorMappedAddress = 0x0020;

// Address families (RFC 8489 Section 14.1).
constexpr uint8_t kFamilyIPv4 = 0x01;
constexpr uint8_t kFamilyIPv6 = 0x02;

using TransactionId = std::array<uint8_t, 12>;

// A single STUN attribute: a 16-bit type plus an opaque value.
struct Attribute {
    uint16_t type = 0;
    std::vector<uint8_t> value;
};

// A STUN message: a 20-byte header followed by zero or more attributes.
struct Message {
    uint16_t type = 0;
    uint32_t magic_cookie = kMagicCookie;
    TransactionId transaction_id{};  // 96-bit transaction id
    std::vector<Attribute> attributes;

    // Serialize to wire format (big-endian, 4-byte attribute padding).
    std::vector<uint8_t> encode() const;

    // Parse from wire format. Throws std::runtime_error on malformed input.
    static Message decode(const std::vector<uint8_t>& bytes);

    void add_attribute(uint16_t type, std::vector<uint8_t> value);
    const Attribute* find(uint16_t type) const;
};

// A host address plus port.
struct Endpoint {
    std::string address;  // dotted-quad IPv4 or textual IPv6
    uint16_t port = 0;
    uint8_t family = kFamilyIPv4;  // kFamilyIPv4 or kFamilyIPv6 (set by decode)
};

// Human-readable family name ("IPv4"/"IPv6").
inline const char* family_name(uint8_t family) {
    return family == kFamilyIPv6 ? "IPv6" : "IPv4";
}

// Encode an endpoint as the value of a XOR-MAPPED-ADDRESS attribute.
std::vector<uint8_t> encode_xor_mapped_address(const Endpoint& endpoint,
                                               const TransactionId& id);

// Decode a MAPPED-ADDRESS (xor_encoded=false) or XOR-MAPPED-ADDRESS
// (xor_encoded=true) attribute value.
Endpoint decode_mapped_address(const std::vector<uint8_t>& value,
                               const TransactionId& id,
                               bool xor_encoded);

}  // namespace stun
