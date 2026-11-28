#include "event_loop.h"

#include <event2/event.h>
#include <event2/util.h>

#include <signal.h>
#include <sys/time.h>

#include <list>
#include <map>
#include <stdexcept>
#include <utility>

namespace framework {

namespace {

void read_trampoline(evutil_socket_t fd, short, void* arg) {
    auto* cb = static_cast<std::function<void(int)>*>(arg);
    (*cb)(static_cast<int>(fd));
}

void timer_trampoline(evutil_socket_t, short, void* arg) {
    auto* cb = static_cast<std::function<void()>*>(arg);
    (*cb)();
}

void signal_trampoline(evutil_socket_t, short, void* arg) {
    event_base_loopbreak(static_cast<struct event_base*>(arg));
}

}  // namespace

struct EventLoop::Impl {
    struct event_base* base = nullptr;

    struct ReadEntry {
        struct event* ev = nullptr;
        std::function<void(int)> cb;
    };
    struct TimerEntry {
        struct event* ev = nullptr;
        std::function<void()> cb;
    };

    // std::map and std::list keep node addresses stable, so the &cb pointers
    // handed to libevent remain valid for the entry's lifetime.
    std::map<int, ReadEntry> reads;
    std::list<TimerEntry> timers;

    explicit Impl() {
        base = event_base_new();
        if (!base) throw std::runtime_error("event_base_new failed");
    }

    ~Impl() {
        for (auto& [fd, e] : reads) {
            if (e.ev) {
                event_del(e.ev);
                event_free(e.ev);
            }
        }
        for (auto& t : timers) {
            if (t.ev) {
                event_del(t.ev);
                event_free(t.ev);
            }
        }
        if (base) event_base_free(base);
    }
};

EventLoop::EventLoop() : impl_(std::make_unique<Impl>()) {}
EventLoop::~EventLoop() = default;

void EventLoop::add_read(int fd, std::function<void(int)> callback) {
    remove_read(fd);
    auto [it, inserted] = impl_->reads.emplace(fd, Impl::ReadEntry{});
    Impl::ReadEntry& entry = it->second;
    entry.cb = std::move(callback);
    entry.ev = event_new(impl_->base, fd, EV_READ | EV_PERSIST, read_trampoline, &entry.cb);
    if (!entry.ev) {
        impl_->reads.erase(it);
        throw std::runtime_error("event_new failed");
    }
    if (event_add(entry.ev, nullptr) < 0) {
        event_free(entry.ev);
        impl_->reads.erase(it);
        throw std::runtime_error("event_add failed");
    }
}

void EventLoop::remove_read(int fd) {
    auto it = impl_->reads.find(fd);
    if (it == impl_->reads.end()) return;
    event_del(it->second.ev);
    event_free(it->second.ev);
    impl_->reads.erase(it);
}

void EventLoop::add_timer(std::chrono::milliseconds interval, std::function<void()> callback) {
    impl_->timers.emplace_back();
    Impl::TimerEntry& entry = impl_->timers.back();
    entry.cb = std::move(callback);
    entry.ev = event_new(impl_->base, -1, EV_TIMEOUT | EV_PERSIST, timer_trampoline, &entry.cb);
    if (!entry.ev) {
        impl_->timers.pop_back();
        throw std::runtime_error("event_new failed");
    }
    timeval tv{};
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(interval).count();
    tv.tv_sec = static_cast<time_t>(us / 1000000);
    tv.tv_usec = static_cast<suseconds_t>(us % 1000000);
    if (event_add(entry.ev, &tv) < 0) {
        event_free(entry.ev);
        impl_->timers.pop_back();
        throw std::runtime_error("event_add failed");
    }
}

void EventLoop::run() {
    struct event* sigint = evsignal_new(impl_->base, SIGINT, signal_trampoline, impl_->base);
    struct event* sigterm = evsignal_new(impl_->base, SIGTERM, signal_trampoline, impl_->base);
    if (sigint) event_add(sigint, nullptr);
    if (sigterm) event_add(sigterm, nullptr);

    event_base_dispatch(impl_->base);

    if (sigint) event_free(sigint);
    if (sigterm) event_free(sigterm);
}

void EventLoop::stop() {
    event_base_loopbreak(impl_->base);
}

}  // namespace framework
