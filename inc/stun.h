// stun.h — minimal STUN (RFC 8489) client library.

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
};

// Build a Binding request with a fresh random transaction id.
Message make_binding_request();

// Encode an endpoint as the value of a XOR-MAPPED-ADDRESS attribute.
std::vector<uint8_t> encode_xor_mapped_address(const Endpoint& endpoint,
                                               const TransactionId& id);

// Decode a MAPPED-ADDRESS (xor_encoded=false) or XOR-MAPPED-ADDRESS
// (xor_encoded=true) attribute value.
Endpoint decode_mapped_address(const std::vector<uint8_t>& value,
                               const TransactionId& id,
                               bool xor_encoded);

// Extract the reflexive/mapped address from a successful Binding response.
std::optional<Endpoint> mapped_address(const Message& response);

}  // namespace stun
