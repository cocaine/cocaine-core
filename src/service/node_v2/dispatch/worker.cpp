#include "cocaine/detail/service/node_v2/dispatch/worker.hpp"

using namespace cocaine;

worker_client_dispatch_t::worker_client_dispatch_t(upstream<outcoming_tag>& stream_,
                                                   close_handler handler_):
    dispatch<incoming_tag>("W2C"),
    stream(std::move(stream_)),
    handler(std::move(handler_))
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
            finalize(nullptr);
        } catch (std::exception& err) {
            finalize(&err);
        }
    });

    on<protocol::choke>([&]() {
        try {
            stream.send<protocol::choke>();
            finalize(nullptr);
        } catch (std::exception& err) {
            finalize(&err);
        }
    });
}

void
worker_client_dispatch_t::discard(const std::error_code&) const {}

void
worker_client_dispatch_t::finalize(std::exception* err) {
    handler(err);
}
