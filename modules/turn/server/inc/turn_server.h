// turn_server.h — TURN relay server: response builders + UDP relay engine.

#pragma once

#include "turn.h"

#include "event_loop.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace stun::turn::server {

// --- Response builders (pure, testable) ------------------------------------

// Error response: echoes the request's method and transaction id with
// ErrorResponse class, carrying an ERROR-CODE attribute.
Message make_error_response(const Message& request, const ErrorCode& err);

// Allocate success: XOR-RELAYED-ADDRESS + XOR-MAPPED-ADDRESS + LIFETIME.
Message make_allocate_response(const Message& request, const Endpoint& relayed,
                               const Endpoint& mapped, uint32_t lifetime);

// Refresh success: LIFETIME only.
Message make_refresh_response(const Message& request, uint32_t lifetime);

// CreatePermission success: no attributes.
Message make_create_permission_response(const Message& request);

// ChannelBind success: echoes CHANNEL-NUMBER.
Message make_channel_bind_response(const Message& request, uint16_t channel);

// Data indication to a client: XOR-PEER-ADDRESS + DATA (fresh transaction id).
Message make_data_indication(const Endpoint& peer, const std::vector<uint8_t>& data);

// --- UDP relay engine -------------------------------------------------------

class TurnServer {
public:
    // `relay_address` is the IP advertised in XOR-RELAYED-ADDRESS.
    TurnServer(std::string relay_address, uint16_t port);
    ~TurnServer();
    TurnServer(const TurnServer&) = delete;
    TurnServer& operator=(const TurnServer&) = delete;

    // Run the event loop until an unrecoverable error; returns an exit code.
    int run();

private:
    struct ClientAddr {
        sockaddr_storage addr{};
        socklen_t len = 0;
    };

    std::string relay_address_;
    uint16_t port_;
    int client_fd_ = -1;
    framework::EventLoop loop_;

    std::map<std::string, Allocation> allocations_;   // client_key -> allocation
    std::map<std::string, int> relay_fds_;            // client_key -> relay fd
    std::map<int, std::string> relay_fd_to_client_;   // relay fd -> client_key
    std::map<std::string, ClientAddr> client_addrs_;  // client_key -> client sockaddr
    std::map<std::string, ClientAddr> peer_addrs_;    // addr_key -> peer sockaddr

    void expire_allocations();
    void remove_allocation(const std::string& client_key);
    int create_relay_socket();

    void handle_client_readable();
    void handle_datagram(const std::vector<uint8_t>& data, const sockaddr* from,
                         socklen_t from_len);
    void handle_stun(const Message& msg, const sockaddr* from, socklen_t from_len);
    void handle_channel_data(const std::vector<uint8_t>& data, const sockaddr* from,
                             socklen_t from_len);
    void handle_allocate(const Message& msg, const sockaddr* from, socklen_t from_len);
    void handle_refresh(const Message& msg, const sockaddr* from, socklen_t from_len);
    void handle_create_permission(const Message& msg, const sockaddr* from,
                                  socklen_t from_len);
    void handle_channel_bind(const Message& msg, const sockaddr* from, socklen_t from_len);
    void handle_send_indication(const Message& msg, const std::string& client_key);
    void handle_peer_data(int relay_fd);

    void send_to(const sockaddr* to, socklen_t to_len, const std::vector<uint8_t>& data);
    void send_response(const Message& response, const sockaddr* to, socklen_t to_len);
};

}  // namespace stun::turn::server
