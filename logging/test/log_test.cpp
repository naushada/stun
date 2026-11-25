#include "log.h"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

void test_levels() {
    logging::init(logging::Backend::Stderr, logging::Level::Notice);

    // Default applies to any module.
    CHECK(logging::get_level("any") == logging::Level::Notice);
    CHECK(!logging::enabled(logging::Level::Debug, "any"));
    CHECK(logging::enabled(logging::Level::Error, "any"));

    // Per-module override.
    logging::set_level("client", logging::Level::Debug);
    CHECK(logging::get_level("client") == logging::Level::Debug);
    CHECK(logging::enabled(logging::Level::Debug, "client"));

    // Unrelated module keeps the default.
    CHECK(logging::get_level("server") == logging::Level::Notice);
    CHECK(!logging::enabled(logging::Level::Debug, "server"));

    // Changing the default affects only modules without an override.
    logging::set_default_level(logging::Level::Error);
    CHECK(logging::get_level("server") == logging::Level::Error);
    CHECK(logging::get_level("client") == logging::Level::Debug);  // still overridden
}

void test_format() {
    CHECK(logging::format("%s:%d", "hello", 42) == "hello:42");
    CHECK(logging::format("no args") == "no args");
}

void test_parse_backend() {
    CHECK(logging::parse_backend("syslog") == logging::Backend::Syslog);
    CHECK(logging::parse_backend("all") == logging::Backend::All);
    CHECK(logging::parse_backend("both") == logging::Backend::All);
    CHECK(logging::parse_backend("stderr") == logging::Backend::Stderr);
    CHECK(logging::parse_backend("garbage") == logging::Backend::Stderr);
}

void test_emit_smoke() {
    // Emitting at enabled levels must not crash (output goes to stderr).
    logging::set_level("test", logging::Level::Info);
    logging::log(logging::Level::Notice, "test", "listening on udp/%u", 3478);
    logging::log(logging::Level::Debug, "test", "this should be filtered out");
    logging::write(logging::Level::Error, "test", "oops");
}

}  // namespace

int main() {
    test_levels();
    test_format();
    test_parse_backend();
    test_emit_smoke();

    if (failures == 0) {
        std::printf("All log tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
