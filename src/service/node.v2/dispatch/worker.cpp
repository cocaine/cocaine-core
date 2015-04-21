#include "cocaine/detail/service/node.v2/dispatch/worker.hpp"

using namespace cocaine;

worker_client_dispatch_t::worker_client_dispatch_t(upstream<io::event_traits<io::app::enqueue>::upstream_type>& stream_,
                                                   std::function<void()> callback):
    dispatch<tag>("w->c"),
    stream(stream_)
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
