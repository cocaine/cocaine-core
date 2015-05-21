#pragma once

#include <functional>

#include <boost/optional/optional.hpp>

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/slot/streamed.hpp"
#include "cocaine/rpc/upstream.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

namespace cocaine {

/// An adapter for [Client -> Worker] message passing.
class streaming_dispatch_t:
    public dispatch<io::event_traits<io::app::enqueue>::dispatch_type>
{
public:
    typedef std::function<void()> close_handler;

private:
    typedef io::event_traits<io::app::enqueue>::dispatch_type incoming_tag;
    typedef io::event_traits<io::worker::rpc::invoke>::dispatch_type outcoming_tag;
    typedef io::protocol<incoming_tag>::scope protocol;

    /// Upstream to the worker.
    streamed<std::string> stream;

    /// On close callback.
    boost::optional<close_handler> handler;

    std::mutex mutex;

    enum class state_t {
        open,
        closed
    };

    state_t state;

public:
    explicit
    streaming_dispatch_t(const std::string& name);

    void
    attach(upstream<outcoming_tag> stream, close_handler handler);

    /// The client has been disconnected without closing its opened channels.
    ///
    /// In this case we should call close callback to prevend resource leak.
    virtual
    void
    discard(const std::error_code& ec) const;

private:
    void
    finalize();
};

} // namespace cocaine
