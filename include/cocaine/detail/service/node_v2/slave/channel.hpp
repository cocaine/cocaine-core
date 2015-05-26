#pragma once

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

    enum state_t {
        none = 0x00,
        tx = 0x01,
        rx = 0x02,
        both = tx | rx
    };

private:
    const std::uint64_t id;
    int state;

    callback_type callback;

    std::mutex mutex;

public:
    channel_t(std::uint64_t id, callback_type callback);

    void
    close(state_t side, const std::error_code& ec);
};

}
