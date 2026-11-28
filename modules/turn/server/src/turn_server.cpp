#include "turn_server.h"

#include "net.h"

#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stun::turn::server {

namespace {

using SteadyClock = std::chrono::steady_clock;

}  // namespace

// --- Response builders ------------------------------------------------------

Message make_error_response(const Message& request, const ErrorCode& err) {
    Message response;
    response.type = make_type(method_of(request.type), MessageClass::ErrorResponse);
    response.transaction_id = request.transaction_id;
    response.add_attribute(kAttrErrorCode, encode_error_code(err));
    return response;
}

Message make_allocate_response(const Message& request, const Endpoint& relayed,
                               const Endpoint& mapped, uint32_t lifetime) {
    Message response;
    response.type = make_type(kMethodAllocate, MessageClass::SuccessResponse);
    response.transaction_id = request.transaction_id;
    response.add_attribute(kAttrXorRelayedAddress,
                           encode_xor_mapped_address(relayed, request.transaction_id));
    response.add_attribute(kAttrXorMappedAddress,
                           encode_xor_mapped_address(mapped, request.transaction_id));
    response.add_attribute(kAttrLifetime, encode_lifetime(lifetime));
    return response;
}

Message make_refresh_response(const Message& request, uint32_t lifetime) {
    Message response;
    response.type = make_type(kMethodRefresh, MessageClass::SuccessResponse);
    response.transaction_id = request.transaction_id;
    response.add_attribute(kAttrLifetime, encode_lifetime(lifetime));
    return response;
}

Message make_create_permission_response(const Message& request) {
    Message response;
    response.type = make_type(kMethodCreatePermission, MessageClass::SuccessResponse);
    response.transaction_id = request.transaction_id;
    return response;
}

Message make_channel_bind_response(const Message& request, uint16_t channel) {
    Message response;
    response.type = make_type(kMethodChannelBind, MessageClass::SuccessResponse);
    response.transaction_id = request.transaction_id;
    response.add_attribute(kAttrChannelNumber, encode_channel_number(channel));
    return response;
}

Message make_data_indication(const Endpoint& peer, const std::vector<uint8_t>& data) {
    Message indication;
    indication.type = make_type(kMethodData, MessageClass::Indication);
    indication.transaction_id = random_transaction_id();
    indication.add_attribute(kAttrXorPeerAddress,
                             encode_xor_mapped_address(peer, indication.transaction_id));
    indication.add_attribute(kAttrData, data);
    return indication;
}

// --- TurnServer -------------------------------------------------------------

TurnServer::TurnServer(std::string relay_address, uint16_t port)
    : relay_address_(std::move(relay_address)), port_(port) {}

TurnServer::~TurnServer() {
    for (const auto& [key, fd] : relay_fds_) close(fd);
    if (client_fd_ >= 0) close(client_fd_);
}

int TurnServer::run() {
    client_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd_ < 0) {
        TURN_LOG_ERROR("turn", "socket failed");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    if (bind(client_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        TURN_LOG_ERROR("turn", "bind failed on udp/%u", static_cast<unsigned>(port_));
        return 1;
    }

    TURN_LOG_NOTICE("turn", "listening on udp/%u, relay address %s",
                    static_cast<unsigned>(port_), relay_address_.c_str());

    loop_.add_read(client_fd_, [this](int) { handle_client_readable(); });
    loop_.add_timer(std::chrono::seconds(1), [this] { expire_allocations(); });

    loop_.run();
    return 0;
}

void TurnServer::handle_client_readable() {
    std::vector<uint8_t> buffer(65536);
    sockaddr_storage from{};
    socklen_t from_len = sizeof(from);
    ssize_t n = recvfrom(client_fd_, buffer.data(), buffer.size(), 0,
                         reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n < 0) {
        TURN_LOG_ERROR("turn", "recvfrom failed");
        return;
    }
    handle_datagram(std::vector<uint8_t>(buffer.begin(), buffer.begin() + n),
                    reinterpret_cast<sockaddr*>(&from), from_len);
}

void TurnServer::expire_allocations() {
    auto now = SteadyClock::now();
    std::vector<std::string> to_remove;
    for (const auto& [key, alloc] : allocations_)
        if (alloc.expired(now)) to_remove.push_back(key);
    for (const auto& key : to_remove) remove_allocation(key);
}

void TurnServer::remove_allocation(const std::string& client_key) {
    auto fit = relay_fds_.find(client_key);
    if (fit != relay_fds_.end()) {
        loop_.remove_read(fit->second);
        relay_fd_to_client_.erase(fit->second);
        close(fit->second);
        relay_fds_.erase(fit);
    }
    allocations_.erase(client_key);
    client_addrs_.erase(client_key);
    TURN_LOG_INFO("turn", "removed allocation for %s", client_key.c_str());
}

int TurnServer::create_relay_socket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;  // ephemeral port
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void TurnServer::handle_datagram(const std::vector<uint8_t>& data, const sockaddr* from,
                                 socklen_t from_len) {
    if (data.empty()) return;
    if (looks_like_channel_data(data[0])) {
        handle_channel_data(data, from, from_len);
        return;
    }

    Message msg;
    try {
        msg = Message::decode(data);
    } catch (const std::exception& e) {
        TURN_LOG_DEBUG("turn", "decode failed: %s", e.what());
        return;
    }
    handle_stun(msg, from, from_len);
}

void TurnServer::handle_stun(const Message& msg, const sockaddr* from, socklen_t from_len) {
    MessageClass cls = class_of(msg.type);
    uint16_t method = method_of(msg.type);

    if (cls == MessageClass::Indication) {
        if (method == kMethodSend) {
            Endpoint client = stun::endpoint_from_sockaddr(from);
            handle_send_indication(msg, addr_key(client));
        }
        return;
    }

    if (cls != MessageClass::Request) return;  // ignore responses and unknown classes

    switch (method) {
    case kMethodAllocate:
        handle_allocate(msg, from, from_len);
        break;
    case kMethodRefresh:
        handle_refresh(msg, from, from_len);
        break;
    case kMethodCreatePermission:
        handle_create_permission(msg, from, from_len);
        break;
    case kMethodChannelBind:
        handle_channel_bind(msg, from, from_len);
        break;
    default:
        send_response(make_error_response(msg, ErrorCode{400, "Bad Request"}), from, from_len);
    }
}

void TurnServer::handle_allocate(const Message& msg, const sockaddr* from, socklen_t from_len) {
    const Attribute* rt = msg.find(kAttrRequestedTransport);
    if (rt == nullptr) {
        send_response(make_error_response(msg, ErrorCode{400, "Bad Request"}), from, from_len);
        return;
    }
    auto proto = decode_requested_transport(rt->value);
    if (!proto.has_value() || *proto != kProtoUdp) {
        send_response(make_error_response(msg, ErrorCode{442, "Unsupported Transport Protocol"}),
                      from, from_len);
        return;
    }

    Endpoint client = stun::endpoint_from_sockaddr(from);
    std::string client_key = addr_key(client);
    if (allocations_.count(client_key)) {
        send_response(make_error_response(msg, ErrorCode{437, "Allocation Mismatch"}), from,
                      from_len);
        return;
    }

    int relay_fd = create_relay_socket();
    if (relay_fd < 0) {
        send_response(make_error_response(msg, ErrorCode{500, "Server Error"}), from, from_len);
        return;
    }

    uint16_t relayed_port = 0;
    sockaddr_in bound{};
    socklen_t bound_len = sizeof(bound);
    if (getsockname(relay_fd, reinterpret_cast<sockaddr*>(&bound), &bound_len) == 0)
        relayed_port = ntohs(bound.sin_port);

    Allocation alloc;
    alloc.client_key = client_key;
    alloc.relayed_port = relayed_port;
    alloc.expires_at = SteadyClock::now() + std::chrono::seconds(kDefaultAllocationLifetime);

    allocations_[client_key] = alloc;
    relay_fds_[client_key] = relay_fd;
    relay_fd_to_client_[relay_fd] = client_key;
    loop_.add_read(relay_fd, [this](int fd) { handle_peer_data(fd); });

    ClientAddr ca;
    std::memcpy(&ca.addr, from, from_len);
    ca.len = from_len;
    client_addrs_[client_key] = ca;

    Endpoint relayed{relay_address_, relayed_port, kFamilyIPv4};
    send_response(make_allocate_response(msg, relayed, client, kDefaultAllocationLifetime), from,
                  from_len);
    TURN_LOG_INFO("turn", "allocated relay %s:%u for %s", relay_address_.c_str(),
                  static_cast<unsigned>(relayed_port), client_key.c_str());
}

void TurnServer::handle_refresh(const Message& msg, const sockaddr* from, socklen_t from_len) {
    Endpoint client = stun::endpoint_from_sockaddr(from);
    std::string client_key = addr_key(client);
    auto it = allocations_.find(client_key);
    if (it == allocations_.end()) {
        send_response(make_error_response(msg, ErrorCode{437, "Allocation Mismatch"}), from,
                      from_len);
        return;
    }

    uint32_t lifetime = kDefaultAllocationLifetime;
    const Attribute* lt = msg.find(kAttrLifetime);
    if (lt) {
        auto v = decode_lifetime(lt->value);
        if (v.has_value()) lifetime = std::min(*v, kMaxLifetime);
    }

    if (lifetime == 0) {
        remove_allocation(client_key);
        send_response(make_refresh_response(msg, 0), from, from_len);
        return;
    }

    it->second.expires_at = SteadyClock::now() + std::chrono::seconds(lifetime);
    send_response(make_refresh_response(msg, lifetime), from, from_len);
}

void TurnServer::handle_create_permission(const Message& msg, const sockaddr* from,
                                          socklen_t from_len) {
    Endpoint client = stun::endpoint_from_sockaddr(from);
    std::string client_key = addr_key(client);
    auto it = allocations_.find(client_key);
    if (it == allocations_.end()) {
        send_response(make_error_response(msg, ErrorCode{437, "Allocation Mismatch"}), from,
                      from_len);
        return;
    }

    auto now = SteadyClock::now();
    bool any = false;
    for (const auto& attr : msg.attributes) {
        if (attr.type != kAttrXorPeerAddress) continue;
        try {
            Endpoint peer = decode_mapped_address(attr.value, msg.transaction_id, true);
            it->second.add_permission(peer, now, std::chrono::seconds(kDefaultPermissionLifetime));
            any = true;
        } catch (const std::exception&) {
            // Malformed peer address: skip this attribute.
        }
    }

    if (!any) {
        send_response(make_error_response(msg, ErrorCode{400, "Bad Request"}), from, from_len);
        return;
    }
    send_response(make_create_permission_response(msg), from, from_len);
}

void TurnServer::handle_channel_bind(const Message& msg, const sockaddr* from,
                                     socklen_t from_len) {
    Endpoint client = stun::endpoint_from_sockaddr(from);
    std::string client_key = addr_key(client);
    auto it = allocations_.find(client_key);
    if (it == allocations_.end()) {
        send_response(make_error_response(msg, ErrorCode{437, "Allocation Mismatch"}), from,
                      from_len);
        return;
    }

    const Attribute* cn = msg.find(kAttrChannelNumber);
    const Attribute* xp = msg.find(kAttrXorPeerAddress);
    if (cn == nullptr || xp == nullptr) {
        send_response(make_error_response(msg, ErrorCode{400, "Bad Request"}), from, from_len);
        return;
    }

    auto channel = decode_channel_number(cn->value);
    if (!channel.has_value() || *channel < kChannelMin || *channel > kChannelMax) {
        send_response(make_error_response(msg, ErrorCode{400, "Bad Request"}), from, from_len);
        return;
    }

    Endpoint peer;
    try {
        peer = decode_mapped_address(xp->value, msg.transaction_id, true);
    } catch (const std::exception&) {
        send_response(make_error_response(msg, ErrorCode{400, "Bad Request"}), from, from_len);
        return;
    }

    auto now = SteadyClock::now();
    if (!it->second.bind_channel(*channel, peer, now,
                                 std::chrono::seconds(kDefaultPermissionLifetime))) {
        send_response(make_error_response(msg, ErrorCode{400, "Bad Request"}), from, from_len);
        return;
    }

    // Remember the peer's address so client->peer ChannelData can be relayed.
    ClientAddr pa;
    if (stun::endpoint_to_sockaddr(peer, pa.addr, pa.len)) peer_addrs_[addr_key(peer)] = pa;

    send_response(make_channel_bind_response(msg, *channel), from, from_len);
}

void TurnServer::handle_send_indication(const Message& msg, const std::string& client_key) {
    auto ait = allocations_.find(client_key);
    if (ait == allocations_.end()) return;

    const Attribute* xp = msg.find(kAttrXorPeerAddress);
    const Attribute* data = msg.find(kAttrData);
    if (xp == nullptr || data == nullptr) return;

    Endpoint peer;
    try {
        peer = decode_mapped_address(xp->value, msg.transaction_id, true);
    } catch (const std::exception&) {
        return;
    }

    auto now = SteadyClock::now();
    if (!ait->second.permitted(peer, now)) return;  // drop

    sockaddr_storage sa;
    socklen_t sa_len = 0;
    if (!stun::endpoint_to_sockaddr(peer, sa, sa_len)) return;

    auto fit = relay_fds_.find(client_key);
    if (fit == relay_fds_.end()) return;
    sendto(fit->second, data->value.data(), data->value.size(), 0,
           reinterpret_cast<sockaddr*>(&sa), sa_len);

    // Cache the peer address and refresh the permission on activity.
    ClientAddr pa;
    std::memcpy(&pa.addr, &sa, sa_len);
    pa.len = sa_len;
    peer_addrs_[addr_key(peer)] = pa;
    ait->second.add_permission(peer, now, std::chrono::seconds(kDefaultPermissionLifetime));
}

void TurnServer::handle_channel_data(const std::vector<uint8_t>& data, const sockaddr* from,
                                     socklen_t from_len) {
    Endpoint client = stun::endpoint_from_sockaddr(from);
    std::string client_key = addr_key(client);
    auto it = allocations_.find(client_key);
    if (it == allocations_.end()) return;

    ChannelData cd;
    try {
        cd = ChannelData::decode(data);
    } catch (const std::exception&) {
        return;
    }

    const std::string* peer_key = it->second.peer_for_channel(cd.channel);
    if (peer_key == nullptr) return;  // unknown channel -> drop

    auto pit = peer_addrs_.find(*peer_key);
    if (pit == peer_addrs_.end()) return;  // no known peer address -> drop

    auto fit = relay_fds_.find(client_key);
    if (fit == relay_fds_.end()) return;
    sendto(fit->second, cd.data.data(), cd.data.size(), 0,
           reinterpret_cast<sockaddr*>(&pit->second.addr), pit->second.len);
}

void TurnServer::handle_peer_data(int relay_fd) {
    auto fit = relay_fd_to_client_.find(relay_fd);
    if (fit == relay_fd_to_client_.end()) return;
    const std::string& client_key = fit->second;

    auto ait = allocations_.find(client_key);
    if (ait == allocations_.end()) return;

    std::vector<uint8_t> buffer(65536);
    sockaddr_storage peer{};
    socklen_t peer_len = sizeof(peer);
    ssize_t n = recvfrom(relay_fd, buffer.data(), buffer.size(), 0,
                         reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (n < 0) return;

    Endpoint peer_ep = stun::endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&peer));
    auto now = SteadyClock::now();
    if (!ait->second.permitted(peer_ep, now)) return;  // drop

    // Cache the peer address and refresh the permission on activity.
    ClientAddr pa;
    std::memcpy(&pa.addr, &peer, peer_len);
    pa.len = peer_len;
    peer_addrs_[addr_key(peer_ep)] = pa;
    ait->second.add_permission(peer_ep, now, std::chrono::seconds(kDefaultPermissionLifetime));

    auto cit = client_addrs_.find(client_key);
    if (cit == client_addrs_.end()) return;

    auto chan = ait->second.channel_for_peer(peer_ep);
    if (chan.has_value()) {
        ChannelData cd;
        cd.channel = *chan;
        cd.data.assign(buffer.begin(), buffer.begin() + n);
        send_to(reinterpret_cast<sockaddr*>(&cit->second.addr), cit->second.len, cd.encode());
    } else {
        Message ind = make_data_indication(
            peer_ep, std::vector<uint8_t>(buffer.begin(), buffer.begin() + n));
        send_response(ind, reinterpret_cast<sockaddr*>(&cit->second.addr), cit->second.len);
    }
}

void TurnServer::send_to(const sockaddr* to, socklen_t to_len,
                         const std::vector<uint8_t>& data) {
    sendto(client_fd_, data.data(), data.size(), 0, to, to_len);
}

void TurnServer::send_response(const Message& response, const sockaddr* to, socklen_t to_len) {
    std::vector<uint8_t> packet = response.encode();
    sendto(client_fd_, packet.data(), packet.size(), 0, to, to_len);
}

}  // namespace stun::turn::server
