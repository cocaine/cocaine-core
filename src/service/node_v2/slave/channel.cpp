#include "cocaine/detail/service/node_v2/slave/channel.hpp"

#include "cocaine/detail/service/node_v2/dispatch/client.hpp"
#include "cocaine/detail/service/node_v2/dispatch/worker.hpp"

using namespace cocaine;

channel_t::channel_t(std::uint64_t id, callback_type callback):
    id(id),
    data{both},
    callback(std::move(callback))
{}

void
channel_t::close(state_t side, const std::error_code& ec) {
    std::lock_guard<std::mutex> lock(mutex);

    if (data.state == none) {
        return;
    }

    data.state &= ~side;

    if (ec) {
        data.state = none;
        callback();
        return;
    }

    if (data.state == none) {
        callback();
    }
}

channel_t::state_t
channel_t::state() const {
    return static_cast<state_t>(data.state);
}
