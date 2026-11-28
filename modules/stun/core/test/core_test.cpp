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
    CHECK(out.family == stun::kFamilyIPv4);
}

void test_xor_mapped_ipv6() {
    stun::TransactionId id{};
    for (size_t i = 0; i < id.size(); ++i) id[i] = static_cast<uint8_t>(0xAA + i);

    stun::Endpoint in{"2001:db8::1", 9};
    std::vector<uint8_t> value = stun::encode_xor_mapped_address(in, id);
    stun::Endpoint out = stun::decode_mapped_address(value, id, /*xor=*/true);
    CHECK(out.address == "2001:db8::1");
    CHECK(out.port == 9);
    CHECK(out.family == stun::kFamilyIPv6);
}

void test_type_encoding() {
    using stun::MessageClass;
    // (method, class, expected wire type) vectors from RFC 8489 §6 and RFC 8656 §5.
    struct Vec {
        uint16_t method;
        MessageClass cls;
        uint16_t type;
    };
    const Vec vecs[] = {
        {0x001, MessageClass::Request,         0x0001},  // Binding request
        {0x001, MessageClass::SuccessResponse, 0x0101},  // Binding success
        {0x001, MessageClass::ErrorResponse,   0x0111},  // Binding error
        {0x003, MessageClass::Request,         0x0003},  // Allocate request
        {0x003, MessageClass::SuccessResponse, 0x0103},  // Allocate success
        {0x006, MessageClass::Indication,      0x0016},  // Send indication
        {0x007, MessageClass::Indication,      0x0017},  // Data indication
        {0x008, MessageClass::Request,         0x0008},  // CreatePermission request
        {0x009, MessageClass::Request,         0x0009},  // ChannelBind request
    };
    for (const auto& v : vecs) {
        CHECK(stun::make_type(v.method, v.cls) == v.type);
        CHECK(stun::method_of(v.type) == v.method);
        CHECK(stun::class_of(v.type) == v.cls);
    }
}

void test_error_code_roundtrip() {
    stun::ErrorCode in{400, "Bad Request"};
    std::vector<uint8_t> value = stun::encode_error_code(in);
    CHECK(value.size() == 4 + in.reason.size());
    CHECK(value[0] == 0 && value[1] == 0);  // reserved
    CHECK(value[2] == 4);                   // class = 400 / 100
    CHECK(value[3] == 0);                   // number = 400 % 100
    CHECK(std::string(value.begin() + 4, value.end()) == "Bad Request");

    stun::ErrorCode out = stun::decode_error_code(value);
    CHECK(out.code == 400);
    CHECK(out.reason == "Bad Request");

    stun::ErrorCode in2{437, "Allocation Mismatch"};
    std::vector<uint8_t> v2 = stun::encode_error_code(in2);
    CHECK(v2[2] == 4 && v2[3] == 37);
    stun::ErrorCode out2 = stun::decode_error_code(v2);
    CHECK(out2.code == 437);
    CHECK(out2.reason == "Allocation Mismatch");
}

}  // namespace

int main() {
    test_message_roundtrip();
    test_xor_mapped_ipv4();
    test_xor_mapped_ipv6();
    test_type_encoding();
    test_error_code_roundtrip();

    if (failures == 0) {
        std::printf("All core tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
