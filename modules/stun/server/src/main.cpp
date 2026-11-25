#include "stun_server.h"

#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

namespace {

// Convert a socket address into a host address plus port.
stun::Endpoint endpoint_from_sockaddr(const sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
        const auto* in = reinterpret_cast<const sockaddr_in*>(addr);
        char host[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &in->sin_addr, host, sizeof(host)) == nullptr)
            return {};
        return stun::Endpoint{host, ntohs(in->sin_port)};
    }
    if (addr->sa_family == AF_INET6) {
        const auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
        char host[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &in6->sin6_addr, host, sizeof(host)) == nullptr)
            return {};
        return stun::Endpoint{host, ntohs(in6->sin6_port)};
    }
    return {};
}

}  // namespace

int main(int argc, char** argv) {
    logging::init();

    uint16_t listen_port =
        (argc > 1) ? static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10)) : 3478;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        STUN_LOG_ERROR("server", "socket failed");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(listen_port);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        STUN_LOG_ERROR("server", "bind failed on udp/%u", static_cast<unsigned>(listen_port));
        close(fd);
        return 1;
    }

    STUN_LOG_NOTICE("server", "listening on udp/%u", static_cast<unsigned>(listen_port));

    std::vector<uint8_t> buffer(2048);
    while (true) {
        sockaddr_storage from{};
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(fd, buffer.data(), buffer.size(), 0,
                             reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n < 0) {
            STUN_LOG_ERROR("server", "recvfrom failed");
            continue;
        }

        stun::Message request;
        try {
            request = stun::Message::decode(
                std::vector<uint8_t>(buffer.begin(), buffer.begin() + n));
        } catch (const std::exception& e) {
            STUN_LOG_ERROR("server", "decode failed: %s", e.what());
            continue;
        }

        if (request.type != stun::kBindingRequest)
            continue;

        stun::Endpoint client = endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&from));
        stun::Message response = stun::server::make_binding_response(request, client);
        std::vector<uint8_t> packet = response.encode();

        sendto(fd, packet.data(), packet.size(), 0,
               reinterpret_cast<sockaddr*>(&from), from_len);

        STUN_LOG_INFO("server", "responded to %s:%u",
                      client.address.c_str(), static_cast<unsigned>(client.port));
    }

    return 0;  // unreachable
}
