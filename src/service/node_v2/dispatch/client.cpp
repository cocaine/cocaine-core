#include "cocaine/detail/service/node_v2/dispatch/client.hpp"

using namespace cocaine;

enqueue_dispatch_t::enqueue_dispatch_t(const std::string& name):
    dispatch<incoming_tag>(format("%s/C2W", name)),
    state(state_t::open)
{
    on<protocol::chunk>([&](const std::string& chunk) {
        stream.write(chunk);
    });

    on<protocol::error>([&](int id, const std::string& reason) {
        stream.abort(id, reason);
        finalize();
    });

    on<protocol::choke>([&] {
        stream.close();
        finalize();
    });
}

void
enqueue_dispatch_t::attach(upstream<outcoming_tag> stream, close_handler handler) {
    this->stream.attach(std::move(stream));

    std::lock_guard<std::mutex> lock(mutex);
    if (state == state_t::closed) {
        handler();
    } else {
        this->handler.reset(std::move(handler));
    }
}

void
enqueue_dispatch_t::discard(const std::error_code& ec) const {
    if (ec) {
        // We need to send error to worker indicating that no other messages will be sent.
        const_cast<enqueue_dispatch_t*>(this)->finalize();
    }
}

void
enqueue_dispatch_t::finalize() {
    std::lock_guard<std::mutex> lock(mutex);

    // Ensure that we call this method only once.
    BOOST_ASSERT(state == state_t::open);

    state = state_t::closed;

    if (handler) {
        handler->operator()();
    }
}
