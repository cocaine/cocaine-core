#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <functional>
#include <system_error>

namespace cocaine {

class channel_t:
    public std::enable_shared_from_this<channel_t>
{
public:
    typedef std::function<void()> callback_type;

private:
    enum side_t {
        none = 0x00,
        tx = 0x01,
        rx = 0x02,
        both = tx | rx
    };

    const std::uint64_t id;
    const callback_type callback;

    std::atomic<int> state;
    bool watched;
    std::mutex mutex;

public:
    channel_t(std::uint64_t id, callback_type callback);

    bool
    closed() const;

    bool
    send_closed() const;

    bool
    recv_closed() const;

    void
    watch();

    void
    close_send();

    void
    close_recv();

    void
    close_both();

private:
    void
    maybe_notify();
};

}
