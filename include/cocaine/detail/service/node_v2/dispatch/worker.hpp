#pragma once

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {

/// An adapter for [Client <- Worker] message passing.
class worker_client_dispatch_t:
    public dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
{
    typedef io::event_traits<io::worker::rpc::invoke>::upstream_type incoming_tag;
    typedef io::event_traits<io::app::enqueue>::upstream_type outcoming_tag;
    typedef io::protocol<incoming_tag>::scope protocol;

    typedef std::function<void(std::exception*)> close_handler;

    upstream<incoming_tag> stream;

public:
    worker_client_dispatch_t(upstream<outcoming_tag>& stream, close_handler handler);
};

} // namespace cocaine
