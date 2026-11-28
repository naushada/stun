#include "stun.h"

#include "byte_order.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>

namespace stun {

using framework::get_u16;
using framework::get_u32;
using framework::put_u16;
using framework::put_u32;

namespace {

constexpr size_t kHeaderSize = 20;

// The 16-byte XOR mask used by XOR-MAPPED-ADDRESS: the magic cookie followed
// by the 96-bit transaction id.
std::array<uint8_t, 16> xor_mask(const TransactionId& id) {
    std::array<uint8_t, 16> mask{};
    mask[0] = static_cast<uint8_t>(kMagicCookie >> 24);
    mask[1] = static_cast<uint8_t>(kMagicCookie >> 16);
    mask[2] = static_cast<uint8_t>(kMagicCookie >> 8);
    mask[3] = static_cast<uint8_t>(kMagicCookie);
    for (size_t i = 0; i < id.size(); ++i) mask[4 + i] = id[i];
    return mask;
}

}  // namespace

std::vector<uint8_t> Message::encode() const {
    uint16_t length = 0;
    for (const auto& attr : attributes) {
        uint16_t padded = static_cast<uint16_t>(4 + attr.value.size());
        padded = static_cast<uint16_t>((padded + 3) & ~uint16_t(3));
        length = static_cast<uint16_t>(length + padded);
    }

    std::vector<uint8_t> out;
    out.reserve(kHeaderSize + length);
    put_u16(out, type);
    put_u16(out, length);
    put_u32(out, magic_cookie);
    out.insert(out.end(), transaction_id.begin(), transaction_id.end());

    for (const auto& attr : attributes) {
        put_u16(out, attr.type);
        put_u16(out, static_cast<uint16_t>(attr.value.size()));
        out.insert(out.end(), attr.value.begin(), attr.value.end());
        while (out.size() % 4 != 0) out.push_back(0);  // pad to 4-byte boundary
    }
    return out;
}

Message Message::decode(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kHeaderSize)
        throw std::runtime_error("STUN message shorter than 20-byte header");
    if ((bytes[0] & 0xc0) != 0)
        throw std::runtime_error("STUN message has non-zero leading bits");

    Message msg;
    msg.type = get_u16(bytes.data());
    uint16_t length = get_u16(bytes.data() + 2);
    msg.magic_cookie = get_u32(bytes.data() + 4);
    std::copy_n(bytes.data() + 8, msg.transaction_id.size(), msg.transaction_id.begin());

    size_t pos = kHeaderSize;
    size_t end = pos + length;
    if (bytes.size() < end)
        throw std::runtime_error("STUN message truncated");

    while (pos + 4 <= end) {
        Attribute attr;
        attr.type = get_u16(bytes.data() + pos);
        uint16_t len = get_u16(bytes.data() + pos + 2);
        pos += 4;
        if (pos + len > end)
            throw std::runtime_error("STUN attribute truncated");
        attr.value.assign(bytes.data() + pos, bytes.data() + pos + len);
        msg.attributes.push_back(std::move(attr));
        pos += len;
        pos = (pos + 3) & ~size_t(3);  // skip padding
    }
    return msg;
}

void Message::add_attribute(uint16_t type, std::vector<uint8_t> value) {
    attributes.push_back(Attribute{type, std::move(value)});
}

const Attribute* Message::find(uint16_t type) const {
    for (const auto& attr : attributes)
        if (attr.type == type) return &attr;
    return nullptr;
}

std::vector<uint8_t> encode_xor_mapped_address(const Endpoint& endpoint,
                                               const TransactionId& id) {
    std::array<uint8_t, 16> raw{};
    uint8_t family;
    size_t addr_len;

    if (inet_pton(AF_INET, endpoint.address.c_str(), raw.data()) == 1) {
        family = 0x01;  // IPv4
        addr_len = 4;
    } else if (inet_pton(AF_INET6, endpoint.address.c_str(), raw.data()) == 1) {
        family = 0x02;  // IPv6
        addr_len = 16;
    } else {
        throw std::runtime_error("invalid IP address: " + endpoint.address);
    }

    std::vector<uint8_t> value;
    value.reserve(4 + addr_len);
    value.push_back(0);
    value.push_back(family);
    put_u16(value, static_cast<uint16_t>(endpoint.port ^ (kMagicCookie >> 16)));

    auto mask = xor_mask(id);
    for (size_t i = 0; i < addr_len; ++i) value.push_back(raw[i] ^ mask[i]);
    return value;
}

Endpoint decode_mapped_address(const std::vector<uint8_t>& value,
                               const TransactionId& id, bool xor_encoded) {
    if (value.size() < 4)
        throw std::runtime_error("mapped address too short");

    uint8_t family = value[1];
    size_t addr_len;
    if (family == 0x01) addr_len = 4;
    else if (family == 0x02) addr_len = 16;
    else throw std::runtime_error("unsupported address family");

    if (value.size() < 4 + addr_len)
        throw std::runtime_error("mapped address truncated");

    uint16_t xport = get_u16(value.data() + 2);
    uint16_t port = xor_encoded ? static_cast<uint16_t>(xport ^ (kMagicCookie >> 16)) : xport;

    std::array<uint8_t, 16> raw{};
    if (xor_encoded) {
        auto mask = xor_mask(id);
        for (size_t i = 0; i < addr_len; ++i) raw[i] = value[4 + i] ^ mask[i];
    } else {
        std::copy_n(value.data() + 4, addr_len, raw.begin());
    }

    char buf[INET6_ADDRSTRLEN];
    int family_af = (family == 0x01) ? AF_INET : AF_INET6;
    if (inet_ntop(family_af, raw.data(), buf, sizeof(buf)) == nullptr)
        throw std::runtime_error("inet_ntop failed");

    return Endpoint{buf, port, family};
}

std::vector<uint8_t> encode_error_code(const ErrorCode& error) {
    std::vector<uint8_t> value;
    value.reserve(4 + error.reason.size());
    value.push_back(0);
    value.push_back(0);
    value.push_back(static_cast<uint8_t>(error.code / 100));
    value.push_back(static_cast<uint8_t>(error.code % 100));
    value.insert(value.end(), error.reason.begin(), error.reason.end());
    return value;
}

ErrorCode decode_error_code(const std::vector<uint8_t>& value) {
    if (value.size() < 4)
        throw std::runtime_error("error code too short");
    uint8_t cls = value[2];
    uint8_t number = value[3];
    if (cls < 3 || cls > 6 || number > 99)
        throw std::runtime_error("invalid error code class/number");
    return ErrorCode{static_cast<uint16_t>(cls * 100 + number),
                     std::string(value.begin() + 4, value.end())};
}

TransactionId random_transaction_id() {
    TransactionId id{};
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    for (auto& b : id) b = static_cast<uint8_t>(dis(gen));
    return id;
}

}  // namespace stun
