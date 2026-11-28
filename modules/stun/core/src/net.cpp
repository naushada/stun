#include "net.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstring>

namespace stun {

Endpoint endpoint_from_sockaddr(const sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
        const auto* in = reinterpret_cast<const sockaddr_in*>(addr);
        char host[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &in->sin_addr, host, sizeof(host)) == nullptr) return {};
        return Endpoint{host, ntohs(in->sin_port), kFamilyIPv4};
    }
    if (addr->sa_family == AF_INET6) {
        const auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
        char host[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &in6->sin6_addr, host, sizeof(host)) == nullptr) return {};
        return Endpoint{host, ntohs(in6->sin6_port), kFamilyIPv6};
    }
    return {};
}

bool endpoint_to_sockaddr(const Endpoint& endpoint, sockaddr_storage& out,
                          socklen_t& out_len) {
    std::memset(&out, 0, sizeof(out));
    if (endpoint.family == kFamilyIPv4) {
        auto* in = reinterpret_cast<sockaddr_in*>(&out);
        in->sin_family = AF_INET;
        in->sin_port = htons(endpoint.port);
        if (inet_pton(AF_INET, endpoint.address.c_str(), &in->sin_addr) != 1) return false;
        out_len = sizeof(*in);
        return true;
    }
    auto* in6 = reinterpret_cast<sockaddr_in6*>(&out);
    in6->sin6_family = AF_INET6;
    in6->sin6_port = htons(endpoint.port);
    if (inet_pton(AF_INET6, endpoint.address.c_str(), &in6->sin6_addr) != 1) return false;
    out_len = sizeof(*in6);
    return true;
}

}  // namespace stun
