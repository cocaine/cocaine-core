#include "cocaine/detail/service/node_v2/slave/channel.hpp"

#include "cocaine/detail/service/node_v2/dispatch/client.hpp"
#include "cocaine/detail/service/node_v2/dispatch/worker.hpp"

using namespace cocaine;

channel_t::channel_t(std::uint64_t id, callback_type callback):
    id(id),
    state(none),
    callback(std::move(callback))
{}

void
channel_t::close(state_t side, const std::error_code& ec) {
    std::lock_guard<std::mutex> lock(mutex);

    BOOST_ASSERT(state != both);

    state |= side;

    if (ec) {
        state = both;
        callback();
        return;
    }

    if (state == both) {
        callback();
    }
}
