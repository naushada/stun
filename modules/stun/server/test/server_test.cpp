#include "stun_server.h"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

void test_binding_response() {
    stun::Message request;
    request.type = stun::kBindingRequest;
    request.transaction_id = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    stun::Endpoint client{"203.0.113.7", 40000};
    stun::Message response = stun::server::make_binding_response(request, client);

    CHECK(response.type == stun::kBindingResponse);
    CHECK(response.transaction_id == request.transaction_id);
    CHECK(response.find(stun::kAttrXorMappedAddress) != nullptr);

    std::vector<uint8_t> bytes = response.encode();
    stun::Message decoded = stun::Message::decode(bytes);
    const stun::Attribute* attr = decoded.find(stun::kAttrXorMappedAddress);
    CHECK(attr != nullptr);
    if (attr) {
        stun::Endpoint ep = stun::decode_mapped_address(attr->value, decoded.transaction_id, true);
        CHECK(ep.address == "203.0.113.7");
        CHECK(ep.port == 40000);
    }
}

}  // namespace

int main() {
    test_binding_response();

    if (failures == 0) {
        std::printf("All server tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
