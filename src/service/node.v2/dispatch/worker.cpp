#include "cocaine/detail/service/node.v2/dispatch/worker.hpp"

using namespace cocaine;

worker_client_dispatch_t::worker_client_dispatch_t(upstream<outcoming_tag>& stream_,
                                                   std::function<void()> callback):
    dispatch<incoming_tag>("W2C"),
    stream(std::move(stream_))
{
    on<protocol::chunk>([&](const std::string& chunk){
        stream = stream.send<protocol::chunk>(chunk);
    });

    on<protocol::error>([=](int id, const std::string& reason){
        stream.send<protocol::error>(id, reason);
        callback();
    });

    on<protocol::choke>([=](){
        stream.send<protocol::choke>();
        callback();
    });
}
