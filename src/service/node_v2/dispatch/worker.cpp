#include "cocaine/detail/service/node_v2/dispatch/worker.hpp"

using namespace cocaine;

worker_client_dispatch_t::worker_client_dispatch_t(upstream<outcoming_tag>& stream_):
    dispatch<incoming_tag>("W2C"),
    stream(std::move(stream_)),
    err{},
    state(state_t::open)
{
    on<protocol::chunk>([&](const std::string& chunk) {
        try {
            stream = stream.send<protocol::chunk>(chunk);
        } catch (std::exception& err) {
            finalize(&err);
        }
    });

    on<protocol::error>([&](int id, const std::string& reason) {
        try {
            stream.send<protocol::error>(id, reason);
            finalize();
        } catch (std::exception& err) {
            finalize(&err);
        }
    });

    on<protocol::choke>([&]() {
        try {
            stream.send<protocol::choke>();
            finalize();
        } catch (std::exception& err) {
            finalize(&err);
        }
    });
}

void
worker_client_dispatch_t::attach(close_handler handler) {
    std::lock_guard<std::mutex> lock(mutex);
    if (state == state_t::closed) {
        handler(err);
    } else {
        this->handler.reset(std::move(handler));
    }
}

void
worker_client_dispatch_t::discard(const std::error_code&) const {}

void
worker_client_dispatch_t::finalize(std::exception* err) {
    std::lock_guard<std::mutex> lock(mutex);

    // Ensure that we call this method only once.
    if (state == state_t::closed) {
        // TODO: Log err.
        return;
    }

    state = state_t::closed;

    if (handler) {
        // TODO: We must to send error to the worker if some error occurred while writing to the
        // client stream, otherwise it may push its messages to the Hell forever.
        handler->operator()(err);
    } else {
        this->err = err;
    }
}
