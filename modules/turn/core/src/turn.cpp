#include "turn.h"

#include "byte_order.h"

#include <stdexcept>

namespace stun::turn {

using framework::get_u16;
using framework::get_u32;
using framework::put_u16;
using framework::put_u32;

std::vector<uint8_t> encode_requested_transport(uint8_t protocol) {
    return {protocol, 0, 0, 0};
}

std::optional<uint8_t> decode_requested_transport(const std::vector<uint8_t>& value) {
    if (value.size() < 4) return std::nullopt;
    return value[0];
}

std::vector<uint8_t> encode_lifetime(uint32_t seconds) {
    std::vector<uint8_t> value;
    value.reserve(4);
    put_u32(value, seconds);
    return value;
}

std::optional<uint32_t> decode_lifetime(const std::vector<uint8_t>& value) {
    if (value.size() < 4) return std::nullopt;
    return get_u32(value.data());
}

std::vector<uint8_t> encode_channel_number(uint16_t channel) {
    std::vector<uint8_t> value;
    value.reserve(4);
    value.push_back(0);
    value.push_back(0);
    put_u16(value, channel);
    return value;
}

std::optional<uint16_t> decode_channel_number(const std::vector<uint8_t>& value) {
    if (value.size() < 4) return std::nullopt;
    return get_u16(value.data() + 2);
}

std::vector<uint8_t> ChannelData::encode() const {
    std::vector<uint8_t> out;
    out.reserve(4 + data.size());
    put_u16(out, channel);
    put_u16(out, static_cast<uint16_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

ChannelData ChannelData::decode(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 4)
        throw std::runtime_error("ChannelData shorter than 4-byte header");
    uint16_t channel = get_u16(bytes.data());
    uint16_t length = get_u16(bytes.data() + 2);
    if (bytes.size() < 4 + static_cast<size_t>(length))
        throw std::runtime_error("ChannelData truncated");
    return ChannelData{channel, std::vector<uint8_t>(bytes.begin() + 4,
                                                     bytes.begin() + 4 + length)};
}

std::string ip_key(const Endpoint& endpoint) {
    return std::to_string(endpoint.family) + "|" + endpoint.address;
}

std::string addr_key(const Endpoint& endpoint) {
    return ip_key(endpoint) + "|" + std::to_string(endpoint.port);
}

bool Allocation::permitted(const Endpoint& peer,
                           std::chrono::steady_clock::time_point now) const {
    auto it = permissions.find(ip_key(peer));
    return it != permissions.end() && it->second > now;
}

void Allocation::add_permission(const Endpoint& peer,
                                std::chrono::steady_clock::time_point now,
                                std::chrono::seconds lifetime) {
    permissions[ip_key(peer)] = now + lifetime;
}

bool Allocation::bind_channel(uint16_t channel, const Endpoint& peer,
                              std::chrono::steady_clock::time_point now,
                              std::chrono::seconds permission_lifetime) {
    const std::string ak = addr_key(peer);
    auto cit = channels.find(channel);
    if (cit != channels.end() && cit->second != ak) return false;
    auto pit = peer_channels.find(ak);
    if (pit != peer_channels.end() && pit->second != channel) return false;

    channels[channel] = ak;
    peer_channels[ak] = channel;
    add_permission(peer, now, permission_lifetime);  // channel implies permission
    return true;
}

void Allocation::prune_expired_permissions(std::chrono::steady_clock::time_point now) {
    for (auto it = permissions.begin(); it != permissions.end();) {
        if (it->second <= now)
            it = permissions.erase(it);
        else
            ++it;
    }
}

const std::string* Allocation::peer_for_channel(uint16_t channel) const {
    auto it = channels.find(channel);
    return it == channels.end() ? nullptr : &it->second;
}

std::optional<uint16_t> Allocation::channel_for_peer(const Endpoint& peer) const {
    auto it = peer_channels.find(addr_key(peer));
    if (it == peer_channels.end()) return std::nullopt;
    return it->second;
}

bool Allocation::expired(std::chrono::steady_clock::time_point now) const {
    return now >= expires_at;
}

}  // namespace stun::turn
