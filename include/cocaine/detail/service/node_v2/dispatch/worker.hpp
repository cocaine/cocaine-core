#pragma once

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {

class channel_t;

/// An adapter for [Client <- Worker] message passing.
class worker_rpc_dispatch_t:
    public dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
{
public:
    typedef std::function<void(const std::error_code&)> callback_type;

private:
    typedef io::event_traits<io::worker::rpc::invoke>::upstream_type incoming_tag;
    typedef io::event_traits<io::app::enqueue>::upstream_type outcoming_tag;
    typedef io::protocol<incoming_tag>::scope protocol;

    upstream<incoming_tag> stream;

    enum class state_t {
        open,
        closed
    };

    state_t state;

    /// On close callback.
    callback_type callback;

    std::mutex mutex;

public:
    worker_rpc_dispatch_t(upstream<outcoming_tag>& stream, callback_type callback);

private:
    void
    finalize(const std::error_code& ec = {});
};

} // namespace cocaine
