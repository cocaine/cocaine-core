#pragma once

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {

class worker_client_dispatch_t:
    public dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
{
    typedef io::event_traits<io::worker::rpc::invoke>::upstream_type tag;
    typedef io::protocol<tag>::scope protocol;

    upstream<tag> stream;

public:
    explicit worker_client_dispatch_t(upstream<io::event_traits<io::app::enqueue>::upstream_type>& stream,
                                      std::function<void()> callback);
};

}
