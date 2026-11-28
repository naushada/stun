#include "turn.h"

#include <chrono>
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

void test_requested_transport() {
    std::vector<uint8_t> v = stun::turn::encode_requested_transport(stun::turn::kProtoUdp);
    CHECK(v.size() == 4);
    CHECK(v[0] == 17);
    CHECK(v[1] == 0 && v[2] == 0 && v[3] == 0);

    auto proto = stun::turn::decode_requested_transport(v);
    CHECK(proto.has_value() && *proto == 17);

    CHECK(!stun::turn::decode_requested_transport({}).has_value());
    CHECK(!stun::turn::decode_requested_transport({0x11, 0x00}).has_value());
}

void test_lifetime() {
    std::vector<uint8_t> v = stun::turn::encode_lifetime(600);
    CHECK(v.size() == 4);
    CHECK(v[0] == 0 && v[1] == 0 && v[2] == 0x02 && v[3] == 0x58);  // 600 == 0x258

    auto s = stun::turn::decode_lifetime(v);
    CHECK(s.has_value() && *s == 600);

    CHECK(!stun::turn::decode_lifetime({}).has_value());
    CHECK(!stun::turn::decode_lifetime({0, 0}).has_value());
}

void test_channel_number() {
    std::vector<uint8_t> v = stun::turn::encode_channel_number(0x4000);
    CHECK(v.size() == 4);
    CHECK(v[0] == 0 && v[1] == 0 && v[2] == 0x40 && v[3] == 0x00);

    auto c = stun::turn::decode_channel_number(v);
    CHECK(c.has_value() && *c == 0x4000);

    CHECK(!stun::turn::decode_channel_number({}).has_value());
}

void test_channel_data() {
    stun::turn::ChannelData cd;
    cd.channel = 0x4001;
    cd.data = {0xde, 0xad, 0xbe, 0xef};

    std::vector<uint8_t> bytes = cd.encode();
    CHECK(bytes.size() == 8);
    CHECK(bytes[0] == 0x40 && bytes[1] == 0x01);
    CHECK(bytes[2] == 0x00 && bytes[3] == 0x04);

    stun::turn::ChannelData out = stun::turn::ChannelData::decode(bytes);
    CHECK(out.channel == 0x4001);
    CHECK(out.data == std::vector<uint8_t>({0xde, 0xad, 0xbe, 0xef}));

    CHECK(stun::turn::looks_like_channel_data(0x40));
    CHECK(stun::turn::looks_like_channel_data(0x7F));
    CHECK(!stun::turn::looks_like_channel_data(0x00));  // STUN (top bits 00)
    CHECK(!stun::turn::looks_like_channel_data(0xBF));  // reserved (top bits 10)

    // Truncated ChannelData must throw.
    bool threw = false;
    try {
        stun::turn::ChannelData::decode({0x40, 0x01, 0x00, 0x08, 0xde});
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

void test_allocation_permissions() {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();
    stun::turn::Allocation alloc;
    stun::Endpoint peer{"192.0.2.1", 50000};
    CHECK(!alloc.permitted(peer, now));

    alloc.add_permission(peer, now, std::chrono::seconds(300));
    CHECK(alloc.permitted(peer, now));

    // Permission is per-IP: a different port on the same IP is still permitted.
    stun::Endpoint peer_other_port{"192.0.2.1", 50001};
    CHECK(alloc.permitted(peer_other_port, now));

    // A different IP is not permitted.
    stun::Endpoint other{"192.0.2.2", 50000};
    CHECK(!alloc.permitted(other, now));

    // Expiry.
    CHECK(!alloc.permitted(peer, now + std::chrono::seconds(301)));

    // Prune removes expired permissions.
    alloc.add_permission(peer, now, std::chrono::seconds(300));
    alloc.prune_expired_permissions(now + std::chrono::seconds(301));
    CHECK(alloc.permissions.empty());
}

void test_allocation_channels() {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();
    stun::turn::Allocation alloc;
    stun::Endpoint peer{"192.0.2.7", 40000};

    CHECK(alloc.bind_channel(0x4000, peer, now, std::chrono::seconds(600)));
    CHECK(alloc.permitted(peer, now));  // channel binding implies permission

    auto ch = alloc.channel_for_peer(peer);
    CHECK(ch.has_value() && *ch == 0x4000);

    const std::string* p = alloc.peer_for_channel(0x4000);
    CHECK(p != nullptr && *p == stun::turn::addr_key(peer));

    // Same channel bound to a different peer must fail.
    stun::Endpoint peer2{"192.0.2.8", 40000};
    CHECK(!alloc.bind_channel(0x4000, peer2, now, std::chrono::seconds(600)));
    // Same peer bound to a different channel must fail.
    CHECK(!alloc.bind_channel(0x4001, peer, now, std::chrono::seconds(600)));

    CHECK(alloc.peer_for_channel(0x4001) == nullptr);
}

void test_allocation_expiry() {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();
    stun::turn::Allocation alloc;
    alloc.expires_at = now + std::chrono::seconds(600);
    CHECK(!alloc.expired(now));
    CHECK(alloc.expired(now + std::chrono::seconds(600)));
    CHECK(alloc.expired(now + std::chrono::seconds(601)));
}

}  // namespace

int main() {
    test_requested_transport();
    test_lifetime();
    test_channel_number();
    test_channel_data();
    test_allocation_permissions();
    test_allocation_channels();
    test_allocation_expiry();

    if (failures == 0) {
        std::printf("All turn core tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
