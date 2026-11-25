#include "log.h"

#include <syslog.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include <openssl/ssl.h>

#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace logging {
namespace {

constexpr int kHttpTimeoutMs = 500;

std::mutex g_mutex;
Backend g_backend = Backend::Stderr;
Level g_default_level = Level::Notice;
int g_facility = LOG_USER;
bool g_syslog_open = false;
std::unordered_map<std::string, Level> g_levels;

std::string g_http_host = "localhost";
uint16_t g_http_port = 9200;
std::string g_http_path = "/";
bool g_http_tls = false;
bool g_http_verify = true;
SSL_CTX* g_ssl_ctx = nullptr;

const char* level_name(Level level) {
    switch (level) {
        case Level::Emergency: return "emergency";
        case Level::Alert:     return "alert";
        case Level::Critical:  return "critical";
        case Level::Error:     return "error";
        case Level::Warning:   return "warning";
        case Level::Notice:    return "notice";
        case Level::Info:      return "info";
        case Level::Debug:     return "debug";
    }
    return "unknown";
}

int syslog_priority(Level level) {
    switch (level) {
        case Level::Emergency: return LOG_EMERG;
        case Level::Alert:     return LOG_ALERT;
        case Level::Critical:  return LOG_CRIT;
        case Level::Error:     return LOG_ERR;
        case Level::Warning:   return LOG_WARNING;
        case Level::Notice:    return LOG_NOTICE;
        case Level::Info:      return LOG_INFO;
        case Level::Debug:     return LOG_DEBUG;
    }
    return LOG_NOTICE;
}

// ISO 8601 UTC timestamp, e.g. 2026-11-25T12:34:56Z.
std::string now_utc() {
    using namespace std::chrono;
    std::time_t secs = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
    gmtime_r(&secs, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string to_json(Level level, std::string_view module, const std::string& message) {
    std::string out;
    out.reserve(message.size() + 128);
    out += "{\"timestamp\":\"";
    out += now_utc();
    out += "\",\"level\":\"";
    out += level_name(level);
    out += "\",\"module\":\"";
    out += json_escape(std::string(module));
    out += "\",\"message\":\"";
    out += json_escape(message);
    out += "\"}";
    return out;
}

void emit_stderr(Level level, std::string_view module, const std::string& message) {
    std::string line = to_json(level, module, message) + "\n";
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
}

void emit_syslog(Level level, std::string_view module, const std::string& message) {
    if (!g_syslog_open) {
        openlog(nullptr, LOG_PID, g_facility);
        g_syslog_open = true;
    }
    std::string module_str(module);
    syslog(syslog_priority(level), "[%s] %s", module_str.c_str(), message.c_str());
}

// TCP connect with a short timeout. Returns a socket fd, or -1 on failure.
int tcp_connect(const std::string& host, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0)
        return -1;

    int fd = -1;
    for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s < 0) continue;

        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);

        int rc = connect(s, ai->ai_addr, ai->ai_addrlen);
        if (rc != 0 && errno != EINPROGRESS) {
            close(s);
            continue;
        }
        if (rc != 0) {  // EINPROGRESS: wait for the connect to finish.
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(s, &wfds);
            timeval tv{kHttpTimeoutMs / 1000, (kHttpTimeoutMs % 1000) * 1000};
            if (select(s + 1, nullptr, &wfds, nullptr, &tv) <= 0) {
                close(s);
                continue;
            }
            int so_error = 0;
            socklen_t slen = sizeof(so_error);
            getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &slen);
            if (so_error != 0) {
                close(s);
                continue;
            }
        }

        fcntl(s, F_SETFL, flags);  // restore blocking mode
        fd = s;
        break;
    }
    freeaddrinfo(res);
    return fd;
}

std::string build_request(const std::string& host, uint16_t port,
                          const std::string& path, const std::string& body) {
    std::string port_str = std::to_string(port);
    std::string request;
    request.reserve(body.size() + 256);
    request += "POST " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + ":" + port_str + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += body;
    return request;
}

bool send_all(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

// Lazily build (and cache) the client SSL context. Assumes g_mutex is held.
SSL_CTX* get_ssl_ctx() {
    if (g_ssl_ctx) return g_ssl_ctx;
    g_ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!g_ssl_ctx) return nullptr;
    if (g_http_verify) {
        SSL_CTX_set_default_verify_paths(g_ssl_ctx);
        SSL_CTX_set_verify(g_ssl_ctx, SSL_VERIFY_PEER, nullptr);
    } else {
        SSL_CTX_set_verify(g_ssl_ctx, SSL_VERIFY_NONE, nullptr);
    }
    return g_ssl_ctx;
}

// Send `request` over an already-connected socket wrapped in TLS. Best-effort.
void https_write(int fd, const std::string& host, const std::string& request) {
    SSL_CTX* ctx = get_ssl_ctx();
    if (!ctx) return;
    SSL* ssl = SSL_new(ctx);
    if (!ssl) return;
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, host.c_str());  // SNI
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        return;
    }
    size_t sent = 0;
    while (sent < request.size()) {
        int n = SSL_write(ssl, request.data() + sent,
                          static_cast<int>(request.size() - sent));
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

void emit_http(Level level, std::string_view module, const std::string& message) {
    int fd = tcp_connect(g_http_host, g_http_port);
    if (fd < 0) return;

    timeval tv{kHttpTimeoutMs / 1000, (kHttpTimeoutMs % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string request = build_request(g_http_host, g_http_port, g_http_path,
                                        to_json(level, module, message));
    if (g_http_tls) {
        https_write(fd, g_http_host, request);
    } else {
        send_all(fd, request);
    }
    close(fd);
}

// Assumes g_mutex is held.
bool enabled_locked(Level level, std::string_view module) {
    auto it = g_levels.find(std::string(module));
    Level effective = (it == g_levels.end()) ? g_default_level : it->second;
    return static_cast<int>(level) <= static_cast<int>(effective);
}

std::string format_v(const char* fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int n = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (n < 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    std::vsnprintf(out.data(), out.size() + 1, fmt, args);
    return out;
}

}  // namespace

void init(Backend backend, Level default_level, int facility) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_backend = backend;
    g_default_level = default_level;
    g_facility = facility;
}

void set_default_level(Level level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_default_level = level;
}

void set_level(std::string_view module, Level level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_levels[std::string(module)] = level;
}

void set_http_endpoint(std::string host, uint16_t port, std::string path,
                       bool use_tls, bool verify_peer) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_http_host = std::move(host);
    g_http_port = port;
    g_http_path = std::move(path);
    g_http_tls = use_tls;
    g_http_verify = verify_peer;
    if (g_ssl_ctx) {  // TLS settings may have changed.
        SSL_CTX_free(g_ssl_ctx);
        g_ssl_ctx = nullptr;
    }
}

Level get_level(std::string_view module) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_levels.find(std::string(module));
    return it == g_levels.end() ? g_default_level : it->second;
}

bool enabled(Level level, std::string_view module) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return enabled_locked(level, module);
}

void write(Level level, std::string_view module, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!enabled_locked(level, module)) return;
    if (g_backend & Backend::Syslog) emit_syslog(level, module, message);
    if (g_backend & Backend::Stderr) emit_stderr(level, module, message);
    if (g_backend & Backend::Http)   emit_http(level, module, message);
}

void log(Level level, std::string_view module, const char* fmt, ...) {
    if (!enabled(level, module)) return;
    va_list args;
    va_start(args, fmt);
    std::string message = format_v(fmt, args);
    va_end(args);
    write(level, module, message);
}

std::string format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::string out = format_v(fmt, args);
    va_end(args);
    return out;
}

}  // namespace logging
