#pragma once

#include <functional>

#include "cocaine/forwards.hpp"
#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

namespace cocaine {

/// An adapter for client -> worker message passing.
class streaming_dispatch_t:
    public dispatch<io::event_traits<io::app::enqueue>::dispatch_type>
{
    typedef io::event_traits<io::app::enqueue>::dispatch_type tag;
    typedef io::protocol<tag>::scope protocol;

    streamed<std::string> stream;
    std::function<void()> callback;
    std::mutex mutex;
    bool closed;

public:
    explicit streaming_dispatch_t(const std::string& name);

    void
    attach(std::shared_ptr<upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>> stream,
           std::function<void()> callback);
};

}
