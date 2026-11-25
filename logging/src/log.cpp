#include "log.h"

#include <syslog.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

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

// Send a plain HTTP/1.1 POST (no TLS) and discard the response. Best-effort:
// on any error the line is dropped. Returns true if the request was fully sent.
bool http_post(const std::string& host, uint16_t port, const std::string& path,
               const std::string& body) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0)
        return false;

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
    if (fd < 0) return false;

    timeval snd{kHttpTimeoutMs / 1000, (kHttpTimeoutMs % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &snd, sizeof(snd));

    std::string request;
    request.reserve(body.size() + 256);
    request += "POST " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + ":" + port_str + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += body;

    bool ok = true;
    size_t sent = 0;
    while (sent < request.size()) {
        ssize_t n = send(fd, request.data() + sent, request.size() - sent, 0);
        if (n <= 0) { ok = false; break; }
        sent += static_cast<size_t>(n);
    }
    close(fd);
    return ok;
}

void emit_http(Level level, std::string_view module, const std::string& message) {
    http_post(g_http_host, g_http_port, g_http_path, to_json(level, module, message));
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

void set_http_endpoint(std::string host, uint16_t port, std::string path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_http_host = std::move(host);
    g_http_port = port;
    g_http_path = std::move(path);
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
