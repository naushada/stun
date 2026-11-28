// net.h — socket-address <-> Endpoint conversion, shared by the servers.

#pragma once

#include "stun.h"

#include <netinet/in.h>
#include <sys/socket.h>

namespace stun {

// Convert a socket address into a host address + port. Returns a default
// (family=0) Endpoint if the address family is unsupported.
Endpoint endpoint_from_sockaddr(const sockaddr* addr);

// Convert an Endpoint into a socket address. Returns false on invalid input.
bool endpoint_to_sockaddr(const Endpoint& endpoint, sockaddr_storage& out,
                          socklen_t& out_len);

}  // namespace stun
