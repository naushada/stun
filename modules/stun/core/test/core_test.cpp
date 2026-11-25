#include "stun.h"

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

void test_message_roundtrip() {
    stun::Message msg;
    msg.type = stun::kBindingResponse;
    msg.transaction_id = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    msg.add_attribute(0x0001, {0xde, 0xad, 0xbe, 0xef});

    std::vector<uint8_t> bytes = msg.encode();
    stun::Message decoded = stun::Message::decode(bytes);

    CHECK(decoded.type == stun::kBindingResponse);
    CHECK(decoded.transaction_id == msg.transaction_id);
    CHECK(decoded.attributes.size() == 1);
    CHECK(decoded.attributes[0].type == 0x0001);
    CHECK(decoded.attributes[0].value == std::vector<uint8_t>({0xde, 0xad, 0xbe, 0xef}));
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

}  // namespace

int main() {
    test_message_roundtrip();
    test_xor_mapped_ipv4();
    test_xor_mapped_ipv6();

    if (failures == 0) {
        std::printf("All core tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
