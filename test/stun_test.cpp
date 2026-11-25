#include "stun.h"

#include <cstdio>
#include <string>
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

void test_binding_roundtrip() {
    stun::Message req = stun::make_binding_request();
    CHECK(req.type == stun::kBindingRequest);

    std::vector<uint8_t> bytes = req.encode();
    CHECK(bytes.size() == 20);  // a plain binding request has no attributes

    stun::Message decoded = stun::Message::decode(bytes);
    CHECK(decoded.type == stun::kBindingRequest);
    CHECK(decoded.transaction_id == req.transaction_id);
    CHECK(decoded.attributes.empty());
}

void test_xor_mapped_ipv4() {
    stun::TransactionId id{};
    for (size_t i = 0; i < id.size(); ++i) id[i] = static_cast<uint8_t>(i);

    stun::Endpoint in{"192.0.2.1", 32853};
    std::vector<uint8_t> value = stun::encode_xor_mapped_address(in, id);
    stun::Endpoint out = stun::decode_mapped_address(value, id, /*xor=*/true);
    CHECK(out.address == "192.0.2.1");
    CHECK(out.port == 32853);
}

void test_xor_mapped_ipv6() {
    stun::TransactionId id{};
    for (size_t i = 0; i < id.size(); ++i) id[i] = static_cast<uint8_t>(0xAA + i);

    stun::Endpoint in{"2001:db8::1", 9};
    std::vector<uint8_t> value = stun::encode_xor_mapped_address(in, id);
    stun::Endpoint out = stun::decode_mapped_address(value, id, /*xor=*/true);
    CHECK(out.address == "2001:db8::1");
    CHECK(out.port == 9);
}

void test_response_mapped_address() {
    stun::Message response;
    response.type = stun::kBindingResponse;
    response.transaction_id = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    response.add_attribute(
        stun::kAttrXorMappedAddress,
        stun::encode_xor_mapped_address({"203.0.113.9", 40000}, response.transaction_id));

    std::vector<uint8_t> bytes = response.encode();
    stun::Message decoded = stun::Message::decode(bytes);
    auto endpoint = stun::mapped_address(decoded);
    CHECK(endpoint.has_value());
    if (endpoint) {
        CHECK(endpoint->address == "203.0.113.9");
        CHECK(endpoint->port == 40000);
    }
}

}  // namespace

int main() {
    test_binding_roundtrip();
    test_xor_mapped_ipv4();
    test_xor_mapped_ipv6();
    test_response_mapped_address();

    if (failures == 0) {
        std::printf("All tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
