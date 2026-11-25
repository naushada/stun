#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

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

void test_emit_smoke() {
    // Emitting at enabled levels must not crash (output goes to stderr).
    logging::set_level("test", logging::Level::Info);
    logging::log(logging::Level::Notice, "test", "listening on udp/%u", 3478);
    logging::log(logging::Level::Debug, "test", "this should be filtered out");
    logging::write(logging::Level::Error, "test", "oops");
}

void test_http_sink() {
    // Spin up a local TCP listener to capture the POST.
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listener >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // ephemeral port
    CHECK(bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    CHECK(listen(listener, 1) == 0);

    socklen_t len = sizeof(addr);
    CHECK(getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &len) == 0);

    logging::set_http_endpoint("127.0.0.1", ntohs(addr.sin_port), "/logs/_doc");
    logging::init(logging::Backend::Http, logging::Level::Notice);

    logging::log(logging::Level::Error, "http", "boom %d", 7);

    // The POST is synchronous, so a connection is now pending.
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(listener, &rfds);
    timeval tv{5, 0};
    CHECK(select(listener + 1, &rfds, nullptr, nullptr, &tv) > 0);

    int conn = accept(listener, nullptr, nullptr);
    CHECK(conn >= 0);

    std::string request;
    char buf[1024];
    ssize_t n;
    while ((n = read(conn, buf, sizeof(buf))) > 0)
        request.append(buf, static_cast<size_t>(n));

    CHECK(request.find("POST /logs/_doc HTTP/1.1") != std::string::npos);
    CHECK(request.find("Content-Type: application/json") != std::string::npos);
    CHECK(request.find("\"level\":\"error\"") != std::string::npos);
    CHECK(request.find("\"message\":\"boom 7\"") != std::string::npos);

    close(conn);
    close(listener);
}

}  // namespace

int main() {
    test_levels();
    test_format();
    test_emit_smoke();
    test_http_sink();

    if (failures == 0) {
        std::printf("All log tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
