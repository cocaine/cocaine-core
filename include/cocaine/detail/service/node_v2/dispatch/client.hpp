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
    typedef io::event_traits<io::app::enqueue>::dispatch_type incoming_tag;
    typedef io::event_traits<io::worker::rpc::invoke>::dispatch_type outcoming_tag;
    typedef io::protocol<incoming_tag>::scope protocol;

    /// Upstream to the worker.
    streamed<std::string> stream;

    /// On close callback.
    boost::optional<std::function<void()>> close;

    std::mutex mutex;
    bool closed;

public:
    explicit
    streaming_dispatch_t(const std::string& name);

    void
    attach(upstream<outcoming_tag> stream, std::function<void()> close);

    /// The client has been disconnected without closing its opened channels.
    ///
    /// In this case we should call close callback to prevend resource leak.
    virtual
    void
    discard(const std::error_code& ec) const;

private:
    void
    notify();
};

} // namespace cocaine
