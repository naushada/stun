#include "stun_client.h"

namespace stun::client {

Message make_binding_request() {
    Message msg;
    msg.type = kBindingRequest;
    msg.transaction_id = random_transaction_id();
    return msg;
}

std::optional<Endpoint> mapped_address(const Message& response) {
    if (const Attribute* attr = response.find(kAttrXorMappedAddress))
        return decode_mapped_address(attr->value, response.transaction_id, true);
    if (const Attribute* attr = response.find(kAttrMappedAddress))
        return decode_mapped_address(attr->value, response.transaction_id, false);
    return std::nullopt;
}

}  // namespace stun::client
