// log.h — minimal leveled logging library with syslog semantics.
//
// Severity levels mirror syslog (RFC 5424). Two backends are provided:
//   * Stderr — single-line JSON. This is the container/cloud-friendly path:
//     tail it with Fluentd/Filebeat/Vector (or a container log driver) and
//     ship to Elasticsearch or any cloud log store. (Default.)
//   * Syslog — traditional syslog(3) output for host deployments.
//
// Defaults: Stderr backend, Notice level. Per-module thresholds via set_level().

#pragma once

#include <syslog.h>

#include <cstdarg>
#include <string>
#include <string_view>

namespace logging {

// Syslog severity levels (RFC 5424).
enum class Level : int {
    Emergency = 0,
    Alert     = 1,
    Critical  = 2,
    Error     = 3,
    Warning   = 4,
    Notice    = 5,
    Info      = 6,
    Debug     = 7,
};

// Output backends (bit flags).
enum Backend : unsigned {
    Syslog = 1u << 0,
    Stderr = 1u << 1,
    All    = Syslog | Stderr,
};

// Configure backends, the default level, and the syslog facility.
// Safe to call more than once (reconfigures).
void init(Backend backend = Backend::Stderr, Level default_level = Level::Notice,
          int facility = LOG_USER);

// Default level for modules that have no explicit level.
void set_default_level(Level level);

// Minimum level for a named module (e.g. "client", "server", "core").
void set_level(std::string_view module, Level level);

// Effective level for a module (its own level, or the default).
Level get_level(std::string_view module);

// True if a message at `level` would be emitted for `module`.
bool enabled(Level level, std::string_view module);

// Emit a pre-formatted message (respects the module's level).
void write(Level level, std::string_view module, const std::string& message);

// Emit a printf-style formatted message (respects the module's level).
void log(Level level, std::string_view module, const char* fmt, ...);

// printf-style formatting helper (C++17 has no std::format).
std::string format(const char* fmt, ...);

}  // namespace logging

// Per-level convenience macros.
#define STUN_LOG_EMERGENCY(module, ...) ::logging::log(::logging::Level::Emergency, module, __VA_ARGS__)
#define STUN_LOG_ALERT(module, ...)     ::logging::log(::logging::Level::Alert, module, __VA_ARGS__)
#define STUN_LOG_CRITICAL(module, ...)  ::logging::log(::logging::Level::Critical, module, __VA_ARGS__)
#define STUN_LOG_ERROR(module, ...)     ::logging::log(::logging::Level::Error, module, __VA_ARGS__)
#define STUN_LOG_WARNING(module, ...)   ::logging::log(::logging::Level::Warning, module, __VA_ARGS__)
#define STUN_LOG_NOTICE(module, ...)    ::logging::log(::logging::Level::Notice, module, __VA_ARGS__)
#define STUN_LOG_INFO(module, ...)      ::logging::log(::logging::Level::Info, module, __VA_ARGS__)
#define STUN_LOG_DEBUG(module, ...)     ::logging::log(::logging::Level::Debug, module, __VA_ARGS__)
