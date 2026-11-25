#include "log.h"

#include <syslog.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>

namespace logging {
namespace {

std::mutex g_mutex;
Backend g_backend = Backend::Stderr;
Level g_default_level = Level::Notice;
int g_facility = LOG_USER;
bool g_syslog_open = false;
std::unordered_map<std::string, Level> g_levels;

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

Backend parse_backend(std::string_view name) {
    if (name == "syslog") return Backend::Syslog;
    if (name == "all" || name == "both") return Backend::All;
    return Backend::Stderr;
}

void init_from_env() {
    const char* env = std::getenv("STUN_LOG_BACKEND");
    init(env ? parse_backend(env) : Backend::Stderr);
}

}  // namespace logging
