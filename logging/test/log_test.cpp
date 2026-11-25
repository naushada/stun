#include "log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstdio>
#include <string>
#include <thread>

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

// Generate a self-signed certificate for "localhost". Returns a server SSL_CTX
// (caller frees with SSL_CTX_free), or nullptr on failure.
SSL_CTX* make_test_server_ctx() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) return nullptr;

    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!kctx) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (EVP_PKEY_keygen_init(kctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048) <= 0 ||
        EVP_PKEY_keygen(kctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(kctx);
        SSL_CTX_free(ctx);
        return nullptr;
    }
    EVP_PKEY_CTX_free(kctx);

    X509* cert = X509_new();
    if (!cert) {
        EVP_PKEY_free(pkey);
        SSL_CTX_free(ctx);
        return nullptr;
    }
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_getm_notBefore(cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert), 3600);
    X509_set_pubkey(cert, pkey);

    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               reinterpret_cast<const unsigned char*>("localhost"),
                               -1, -1, 0);
    X509_set_issuer_name(cert, name);
    X509_sign(cert, pkey, EVP_sha256());

    SSL_CTX_use_certificate(ctx, cert);
    SSL_CTX_use_PrivateKey(ctx, pkey);

    X509_free(cert);
    EVP_PKEY_free(pkey);
    return ctx;
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

void test_https_sink() {
    SSL_CTX* server_ctx = make_test_server_ctx();
    CHECK(server_ctx != nullptr);
    if (!server_ctx) return;

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

    logging::set_http_endpoint("127.0.0.1", ntohs(addr.sin_port), "/logs/_doc",
                               /*use_tls=*/true, /*verify_peer=*/false);
    logging::init(logging::Backend::Http, logging::Level::Notice);

    // The TLS handshake needs both sides, so the server runs in a thread.
    std::string request;
    std::thread server([&]() {
        int conn = accept(listener, nullptr, nullptr);
        if (conn >= 0) {
            SSL* ssl = SSL_new(server_ctx);
            SSL_set_fd(ssl, conn);
            if (SSL_accept(ssl) == 1) {
                char buf[1024];
                int n;
                while ((n = SSL_read(ssl, buf, sizeof(buf))) > 0)
                    request.append(buf, static_cast<size_t>(n));
            }
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(conn);
        }
        close(listener);
    });

    logging::log(logging::Level::Error, "https", "secure %d", 9);

    server.join();

    CHECK(request.find("POST /logs/_doc HTTP/1.1") != std::string::npos);
    CHECK(request.find("\"level\":\"error\"") != std::string::npos);
    CHECK(request.find("\"message\":\"secure 9\"") != std::string::npos);

    SSL_CTX_free(server_ctx);
}

}  // namespace

int main() {
    test_levels();
    test_format();
    test_emit_smoke();
    test_http_sink();
    test_https_sink();

    if (failures == 0) {
        std::printf("All log tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
