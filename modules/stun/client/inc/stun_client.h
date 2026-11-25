// stun_client.h — client-side helpers for the STUN core.

#pragma once

#include "stun.h"

#include <optional>
#include <vector>

namespace stun::client {

// Build a Binding request with a fresh random transaction id.
Message make_binding_request();

// Extract the reflexive/mapped address from a successful Binding response.
std::optional<Endpoint> mapped_address(const Message& response);

}  // namespace stun::client
