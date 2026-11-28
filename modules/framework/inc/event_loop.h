// event_loop.h — a small event loop over libevent: datagram read-notification
// events and periodic timers. Hides the event backend (epoll/kqueue/select/…)
// and platform details from the rest of the code.

#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace framework {

class EventLoop {
public:
    EventLoop();
    ~EventLoop();
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Register a persistent read event on `fd`; `callback` is invoked with the
    // file descriptor each time it becomes readable. Replaces any prior event
    // for the same fd.
    void add_read(int fd, std::function<void(int)> callback);

    // Remove the read event for `fd` (safe to call when none is registered).
    void remove_read(int fd);

    // Register a periodic timer; `callback` fires every `interval`.
    void add_timer(std::chrono::milliseconds interval, std::function<void()> callback);

    // Run the loop until stop() is called or SIGINT/SIGTERM is received.
    void run();

    // Request the running loop to stop.
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace framework
