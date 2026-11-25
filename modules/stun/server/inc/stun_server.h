// stun_server.h — server-side helpers for the STUN core.

#pragma once

#include "stun.h"

namespace stun::server {

// Build a Binding success response for the given request, echoing the request's
// transaction id and advertising `client_endpoint` (the address the server saw
// the request come from) in a XOR-MAPPED-ADDRESS attribute.
Message make_binding_response(const Message& request, const Endpoint& client_endpoint);

}  // namespace stun::server
