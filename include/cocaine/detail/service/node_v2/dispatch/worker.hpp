#pragma once

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {

/// An adapter for [Client <- Worker] message passing.
class worker_client_dispatch_t:
    public dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
{
public:
    typedef std::function<void(std::exception*)> close_handler;

private:
    typedef io::event_traits<io::worker::rpc::invoke>::upstream_type incoming_tag;
    typedef io::event_traits<io::app::enqueue>::upstream_type outcoming_tag;
    typedef io::protocol<incoming_tag>::scope protocol;

    upstream<incoming_tag> stream;

    /// On close callback.
    boost::optional<close_handler> handler;
    std::exception* err;

    enum class state_t {
        open,
        closed
    };

    state_t state;

    std::mutex mutex;

public:
    explicit
    worker_client_dispatch_t(upstream<outcoming_tag>& stream);

    void
    attach(close_handler handler);

    virtual
    void
    discard(const std::error_code& ec) const;

private:
    void
    finalize(std::exception* err = nullptr);
};

} // namespace cocaine
