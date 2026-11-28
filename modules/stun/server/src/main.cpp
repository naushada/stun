#include "stun_server.h"

#include "event_loop.h"
#include "net.h"

#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    logging::init_from_env();

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

    framework::EventLoop loop;
    loop.add_read(fd, [fd](int) {
        std::vector<uint8_t> buffer(2048);
        sockaddr_storage from{};
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(fd, buffer.data(), buffer.size(), 0,
                             reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n < 0) {
            STUN_LOG_ERROR("server", "recvfrom failed");
            return;
        }

        stun::Message request;
        try {
            request = stun::Message::decode(
                std::vector<uint8_t>(buffer.begin(), buffer.begin() + n));
        } catch (const std::exception& e) {
            STUN_LOG_ERROR("server", "decode failed: %s", e.what());
            return;
        }

        if (request.type != stun::kBindingRequest) return;

        stun::Endpoint client = stun::endpoint_from_sockaddr(reinterpret_cast<sockaddr*>(&from));
        stun::Message response = stun::server::make_binding_response(request, client);
        std::vector<uint8_t> packet = response.encode();

        sendto(fd, packet.data(), packet.size(), 0,
               reinterpret_cast<sockaddr*>(&from), from_len);

        STUN_LOG_INFO("server", "responded to %s:%u",
                      client.address.c_str(), static_cast<unsigned>(client.port));
    });

    loop.run();
    return 0;
}
