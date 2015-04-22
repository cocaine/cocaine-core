#include "cocaine/detail/service/node.v2/dispatch/client.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

streaming_dispatch_t::streaming_dispatch_t(const std::string& name):
    dispatch<incoming_tag>(format("%s/C2W", name)),
    closed(false)
{
    on<protocol::chunk>([&](const std::string& chunk){
        stream.write(chunk);
    });

    on<protocol::error>([&](int id, const std::string& reason){
        stream.abort(id, reason);
        notify();
    });

    on<protocol::choke>([&]{
        stream.close();
        notify();
    });
}

void
streaming_dispatch_t::attach(std::shared_ptr<upstream<outcoming_tag>> stream, std::function<void()> close) {
    this->stream.attach(std::move(*stream));

    std::lock_guard<std::mutex> lock(mutex);
    if (closed) {
        close();
    } else {
        this->close.reset(std::move(close));
    }
}

void
streaming_dispatch_t::notify() {
    std::lock_guard<std::mutex> lock(mutex);
    BOOST_ASSERT(!closed);

    closed = true;
    if (close) {
        (*close)();
    }
}
