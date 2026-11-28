// turn.h — TURN (RFC 8656) message codecs and allocation state, built on the
// shared STUN core. Pure and socket-free; the relay server lives in ../server.

#pragma once

#include "stun.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace stun::turn {

// TURN methods (RFC 8656 Section 5).
constexpr uint16_t kMethodAllocate         = 0x003;
constexpr uint16_t kMethodRefresh          = 0x004;
constexpr uint16_t kMethodSend             = 0x006;
constexpr uint16_t kMethodData             = 0x007;
constexpr uint16_t kMethodCreatePermission = 0x008;
constexpr uint16_t kMethodChannelBind      = 0x009;

// TURN attribute types (RFC 8656 Section 14).
constexpr uint16_t kAttrChannelNumber      = 0x000C;
constexpr uint16_t kAttrLifetime           = 0x000D;
constexpr uint16_t kAttrXorPeerAddress     = 0x0012;
constexpr uint16_t kAttrData               = 0x0013;
constexpr uint16_t kAttrXorRelayedAddress  = 0x0016;
constexpr uint16_t kAttrRequestedTransport = 0x0019;

// Transport protocol number for UDP (RFC 8656 Section 14.19).
constexpr uint8_t kProtoUdp = 17;

// Channel number range (RFC 8656 Section 14.1).
constexpr uint16_t kChannelMin = 0x4000;
constexpr uint16_t kChannelMax = 0x7FFF;

// Default lifetimes (RFC 8656 Section 6.2).
constexpr uint32_t kDefaultAllocationLifetime = 600;
constexpr uint32_t kDefaultPermissionLifetime = 300;
constexpr uint32_t kMaxLifetime               = 3600;

// --- REQUESTED-TRANSPORT (0x0019) -------------------------------------------
// Value: 1 byte transport protocol, followed by 3 reserved (zero) bytes.
std::vector<uint8_t> encode_requested_transport(uint8_t protocol);

// Returns the protocol, or nullopt if the value is too short.
std::optional<uint8_t> decode_requested_transport(const std::vector<uint8_t>& value);

// --- LIFETIME (0x000D) ------------------------------------------------------
// Value: 4-byte big-endian number of seconds.
std::vector<uint8_t> encode_lifetime(uint32_t seconds);

// Returns the lifetime, or nullopt if the value is too short.
std::optional<uint32_t> decode_lifetime(const std::vector<uint8_t>& value);

// --- CHANNEL-NUMBER (0x000C) ------------------------------------------------
// Value: 2 reserved (zero) bytes, then a 2-byte big-endian channel number.
std::vector<uint8_t> encode_channel_number(uint16_t channel);

// Returns the channel, or nullopt if the value is too short.
std::optional<uint16_t> decode_channel_number(const std::vector<uint8_t>& value);

// XOR-PEER-ADDRESS (0x0012) and XOR-RELAYED-ADDRESS (0x0016) reuse the core
// XOR address codec (encode_xor_mapped_address / decode_mapped_address).

// --- ChannelData message (RFC 8656 Section 12) ------------------------------
// Not a STUN message: a 4-byte header (channel number, length) + application
// data. No padding. Demultiplexed from STUN by the top two bits of byte 0.
struct ChannelData {
    uint16_t channel = 0;
    std::vector<uint8_t> data;

    std::vector<uint8_t> encode() const;
    static ChannelData decode(const std::vector<uint8_t>& bytes);  // throws
};

// True if a message's first byte looks like ChannelData (top two bits 01)
// rather than a STUN message (top two bits 00).
constexpr bool looks_like_channel_data(uint8_t first_byte) {
    return (first_byte & 0xC0u) == 0x40u;
}

// --- Identity keys ----------------------------------------------------------
// Permission key: peer IP address (RFC 8656 Section 2.3 ignores the port).
std::string ip_key(const Endpoint& endpoint);

// Channel-binding key: full peer transport address (IP + port).
std::string addr_key(const Endpoint& endpoint);

// --- Allocation state (socket-free) -----------------------------------------
struct Allocation {
    std::string client_key;   // addr_key of the owning client
    uint16_t relayed_port = 0;
    std::chrono::steady_clock::time_point expires_at{};

    std::map<std::string, std::chrono::steady_clock::time_point> permissions;  // ip_key -> expiry
    std::map<uint16_t, std::string> channels;                                  // channel -> addr_key
    std::map<std::string, uint16_t> peer_channels;                             // addr_key -> channel

    // True if the peer's IP has an unexpired permission.
    bool permitted(const Endpoint& peer, std::chrono::steady_clock::time_point now) const;

    // Install or refresh a permission for the peer's IP.
    void add_permission(const Endpoint& peer, std::chrono::steady_clock::time_point now,
                        std::chrono::seconds lifetime);

    // Bind a channel to a peer's full transport address, also installing an
    // implied permission for the peer's IP. Returns false if the channel or the
    // peer is already bound to something else.
    bool bind_channel(uint16_t channel, const Endpoint& peer,
                      std::chrono::steady_clock::time_point now,
                      std::chrono::seconds permission_lifetime);

    // Remove expired permissions (channel bindings live as long as the
    // allocation and are not independently expired here).
    void prune_expired_permissions(std::chrono::steady_clock::time_point now);

    // Peer transport address (addr_key) bound to a channel, or nullptr.
    const std::string* peer_for_channel(uint16_t channel) const;

    // Channel bound to a peer's transport address, if any.
    std::optional<uint16_t> channel_for_peer(const Endpoint& peer) const;

    // True if the allocation has expired as of `now`.
    bool expired(std::chrono::steady_clock::time_point now) const;
};

}  // namespace stun::turn
