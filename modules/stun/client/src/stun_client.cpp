#include "stun_client.h"

#include <cstdint>
#include <random>

namespace stun::client {

Message make_binding_request() {
    Message msg;
    msg.type = kBindingRequest;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    for (auto& byte : msg.transaction_id) byte = static_cast<uint8_t>(dis(gen));
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
