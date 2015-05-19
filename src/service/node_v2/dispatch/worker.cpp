#include "cocaine/detail/service/node_v2/dispatch/worker.hpp"

using namespace cocaine;

worker_client_dispatch_t::worker_client_dispatch_t(upstream<outcoming_tag>& stream_,
                                                   std::function<void()> callback):
    dispatch<incoming_tag>("W2C"),
    stream(std::move(stream_))
{
    on<protocol::chunk>([&](const std::string& chunk) {
        try {
            stream = stream.send<protocol::chunk>(chunk);
        } catch (const std::exception&) {
            // TODO: Log.
        }
    });

    on<protocol::error>([=](int id, const std::string& reason) {
        try {
            stream.send<protocol::error>(id, reason);
        } catch (const std::exception&) {
            // TODO: Log.
        }

        callback();
    });

    on<protocol::choke>([=]() {
        try {
            stream.send<protocol::choke>();
        } catch (const std::exception&) {
            // TODO: Log.
        }

        callback();
    });
}
