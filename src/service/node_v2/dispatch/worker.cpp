#include "cocaine/detail/service/node_v2/dispatch/worker.hpp"

using namespace cocaine;

worker_client_dispatch_t::worker_client_dispatch_t(upstream<outcoming_tag>& stream_,
                                                   close_handler handler):
    dispatch<incoming_tag>("W2C"),
    stream(std::move(stream_))
{
    on<protocol::chunk>([=](const std::string& chunk) {
        try {
            stream = stream.send<protocol::chunk>(chunk);
        } catch (std::exception& err) {
            handler(&err);
        }
    });

    on<protocol::error>([=](int id, const std::string& reason) {
        try {
            stream.send<protocol::error>(id, reason);
            handler(nullptr);
        } catch (std::exception& err) {
            handler(&err);
        }
    });

    on<protocol::choke>([=]() {
        try {
            stream.send<protocol::choke>();
            handler(nullptr);
        } catch (std::exception& err) {
            handler(&err);
        }
    });
}
