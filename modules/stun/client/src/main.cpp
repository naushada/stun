#include "stun_client.h"

#include "log.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Server {
    std::string host;
    std::string port;
};

// Parse "host:port", "[v6]:port", or a bare host (default port applied).
Server parse_server(const std::string& arg, const std::string& default_port) {
    if (!arg.empty() && arg.front() == '[') {
        auto close = arg.find(']');
        if (close != std::string::npos) {
            std::string port = default_port;
            if (close + 1 < arg.size() && arg[close + 1] == ':')
                port = arg.substr(close + 2);
            return {arg.substr(1, close - 1), port};
        }
    }
    auto colon = arg.rfind(':');
    if (colon != std::string::npos && arg.find(':') == colon)
        return {arg.substr(0, colon), arg.substr(colon + 1)};
    return {arg, default_port};
}

}  // namespace

int main(int argc, char** argv) {
    logging::init_from_env();

    Server server = parse_server((argc > 1) ? argv[1] : "stun.l.google.com:19302", "19302");

    stun::Message request = stun::client::make_binding_request();
    std::vector<uint8_t> packet = request.encode();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* res = nullptr;
    int rc = getaddrinfo(server.host.c_str(), server.port.c_str(), &hints, &res);
    if (rc != 0) {
        STUN_LOG_ERROR("client", "resolve failed: %s", gai_strerror(rc));
        return 1;
    }

    int fd = -1;
    addrinfo* ai = res;
    for (; ai != nullptr; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd >= 0) break;
    }
    if (fd < 0) {
        STUN_LOG_ERROR("client", "socket failed");
        freeaddrinfo(res);
        return 1;
    }

    if (sendto(fd, packet.data(), packet.size(), 0, ai->ai_addr, ai->ai_addrlen) < 0) {
        STUN_LOG_ERROR("client", "send failed");
        freeaddrinfo(res);
        close(fd);
        return 1;
    }
    freeaddrinfo(res);

    timeval timeout{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    std::vector<uint8_t> buffer(2048);
    sockaddr_storage from{};
    socklen_t from_len = sizeof(from);
    ssize_t n = recvfrom(fd, buffer.data(), buffer.size(), 0,
                         reinterpret_cast<sockaddr*>(&from), &from_len);
    close(fd);

    if (n < 0) {
        STUN_LOG_ERROR("client", "no response from %s:%s (timeout or network error)",
                       server.host.c_str(), server.port.c_str());
        return 1;
    }
    buffer.resize(static_cast<size_t>(n));

    stun::Message response;
    try {
        response = stun::Message::decode(buffer);
    } catch (const std::exception& e) {
        STUN_LOG_ERROR("client", "decode failed: %s", e.what());
        return 1;
    }

    if (response.transaction_id != request.transaction_id) {
        STUN_LOG_ERROR("client", "transaction id mismatch");
        return 1;
    }

    if (response.type == stun::kBindingResponse) {
        auto endpoint = stun::client::mapped_address(response);
        if (!endpoint) {
            STUN_LOG_ERROR("client", "binding response has no mapped address");
            return 1;
        }
        // Machine-readable result on stdout; diagnostics go through the logger.
        std::cout << endpoint->address << ":" << endpoint->port << "\n";
        STUN_LOG_NOTICE("client", "mapped address: %s:%u",
                        endpoint->address.c_str(), static_cast<unsigned>(endpoint->port));
        return 0;
    }

    STUN_LOG_ERROR("client", "unexpected message type 0x%04x",
                   static_cast<unsigned>(response.type));
    return 1;
}
