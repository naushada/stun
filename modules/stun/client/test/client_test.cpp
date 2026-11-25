#include "stun_client.h"

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

void test_binding_request() {
    stun::Message req = stun::client::make_binding_request();
    CHECK(req.type == stun::kBindingRequest);

    std::vector<uint8_t> bytes = req.encode();
    CHECK(bytes.size() == 20);  // a plain binding request has no attributes

    stun::Message decoded = stun::Message::decode(bytes);
    CHECK(decoded.transaction_id == req.transaction_id);
}

void test_mapped_address() {
    stun::Message response;
    response.type = stun::kBindingResponse;
    response.transaction_id = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    response.add_attribute(
        stun::kAttrXorMappedAddress,
        stun::encode_xor_mapped_address({"203.0.113.9", 40000}, response.transaction_id));

    auto endpoint = stun::client::mapped_address(response);
    CHECK(endpoint.has_value());
    if (endpoint) {
        CHECK(endpoint->address == "203.0.113.9");
        CHECK(endpoint->port == 40000);
    }
}

}  // namespace

int main() {
    test_binding_request();
    test_mapped_address();

    if (failures == 0) {
        std::printf("All client tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
