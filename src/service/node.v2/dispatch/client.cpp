#include "cocaine/detail/service/node.v2/dispatch/client.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

streaming_dispatch_t::streaming_dispatch_t(const std::string& name):
    dispatch<tag>(name)
{
    on<protocol::chunk>([&](const std::string& chunk){
        stream.write(chunk);
    });

    on<protocol::error>([&](int id, const std::string& reason){
        stream.abort(id, reason);
    });

    on<protocol::choke>([&]{
        stream.close();
    });
}

void
streaming_dispatch_t::attach(std::shared_ptr<upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>> stream) {
    this->stream.attach(std::move(*stream));
}
