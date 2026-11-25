#include "stun_server.h"

namespace stun::server {

Message make_binding_response(const Message& request, const Endpoint& client_endpoint) {
    Message response;
    response.type = kBindingResponse;
    response.transaction_id = request.transaction_id;
    response.add_attribute(kAttrXorMappedAddress,
                           encode_xor_mapped_address(client_endpoint, request.transaction_id));
    return response;
}

}  // namespace stun::server
