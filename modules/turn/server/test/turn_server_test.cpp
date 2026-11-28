#include "turn_server.h"

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

stun::Message make_request(uint16_t method) {
    stun::Message msg;
    msg.type = stun::make_type(method, stun::MessageClass::Request);
    msg.transaction_id = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    return msg;
}

void test_error_response() {
    stun::Message req = make_request(stun::turn::kMethodAllocate);
    stun::Message resp = stun::turn::server::make_error_response(req, {400, "Bad Request"});

    CHECK(resp.type == stun::make_type(stun::turn::kMethodAllocate,
                                       stun::MessageClass::ErrorResponse));
    CHECK(resp.transaction_id == req.transaction_id);

    const stun::Attribute* attr = resp.find(stun::kAttrErrorCode);
    CHECK(attr != nullptr);
    if (attr) {
        stun::ErrorCode err = stun::decode_error_code(attr->value);
        CHECK(err.code == 400);
        CHECK(err.reason == "Bad Request");
    }
}

void test_allocate_response() {
    stun::Message req = make_request(stun::turn::kMethodAllocate);
    stun::Endpoint relayed{"127.0.0.1", 50000};
    stun::Endpoint mapped{"203.0.113.9", 40000};

    stun::Message resp =
        stun::turn::server::make_allocate_response(req, relayed, mapped, 600);

    CHECK(resp.type == stun::make_type(stun::turn::kMethodAllocate,
                                       stun::MessageClass::SuccessResponse));
    CHECK(resp.transaction_id == req.transaction_id);

    const stun::Attribute* r = resp.find(stun::turn::kAttrXorRelayedAddress);
    CHECK(r != nullptr);
    if (r) {
        stun::Endpoint e = stun::decode_mapped_address(r->value, resp.transaction_id, true);
        CHECK(e.address == "127.0.0.1");
        CHECK(e.port == 50000);
    }

    const stun::Attribute* m = resp.find(stun::kAttrXorMappedAddress);
    CHECK(m != nullptr);
    if (m) {
        stun::Endpoint e = stun::decode_mapped_address(m->value, resp.transaction_id, true);
        CHECK(e.address == "203.0.113.9");
        CHECK(e.port == 40000);
    }

    const stun::Attribute* l = resp.find(stun::turn::kAttrLifetime);
    CHECK(l != nullptr);
    if (l) {
        auto s = stun::turn::decode_lifetime(l->value);
        CHECK(s.has_value() && *s == 600);
    }
}

void test_refresh_response() {
    stun::Message req = make_request(stun::turn::kMethodRefresh);
    stun::Message resp = stun::turn::server::make_refresh_response(req, 0);

    CHECK(resp.type == stun::make_type(stun::turn::kMethodRefresh,
                                       stun::MessageClass::SuccessResponse));
    const stun::Attribute* l = resp.find(stun::turn::kAttrLifetime);
    CHECK(l != nullptr);
    if (l) {
        auto s = stun::turn::decode_lifetime(l->value);
        CHECK(s.has_value() && *s == 0);
    }
}

void test_create_permission_response() {
    stun::Message req = make_request(stun::turn::kMethodCreatePermission);
    stun::Message resp = stun::turn::server::make_create_permission_response(req);

    CHECK(resp.type == stun::make_type(stun::turn::kMethodCreatePermission,
                                       stun::MessageClass::SuccessResponse));
    CHECK(resp.attributes.empty());
}

void test_channel_bind_response() {
    stun::Message req = make_request(stun::turn::kMethodChannelBind);
    stun::Message resp = stun::turn::server::make_channel_bind_response(req, 0x4000);

    CHECK(resp.type == stun::make_type(stun::turn::kMethodChannelBind,
                                       stun::MessageClass::SuccessResponse));
    const stun::Attribute* c = resp.find(stun::turn::kAttrChannelNumber);
    CHECK(c != nullptr);
    if (c) {
        auto ch = stun::turn::decode_channel_number(c->value);
        CHECK(ch.has_value() && *ch == 0x4000);
    }
}

void test_data_indication() {
    stun::Endpoint peer{"203.0.113.5", 43210};
    std::vector<uint8_t> data{1, 2, 3, 4};

    stun::Message ind = stun::turn::server::make_data_indication(peer, data);

    CHECK(ind.type == stun::make_type(stun::turn::kMethodData, stun::MessageClass::Indication));

    const stun::Attribute* p = ind.find(stun::turn::kAttrXorPeerAddress);
    CHECK(p != nullptr);
    if (p) {
        stun::Endpoint e = stun::decode_mapped_address(p->value, ind.transaction_id, true);
        CHECK(e.address == "203.0.113.5");
        CHECK(e.port == 43210);
    }

    const stun::Attribute* d = ind.find(stun::turn::kAttrData);
    CHECK(d != nullptr && d->value == data);
}

}  // namespace

int main() {
    test_error_response();
    test_allocate_response();
    test_refresh_response();
    test_create_permission_response();
    test_channel_bind_response();
    test_data_indication();

    if (failures == 0) {
        std::printf("All turn server tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
