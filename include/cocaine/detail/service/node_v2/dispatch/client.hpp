#pragma once

#include <functional>

#include <boost/optional/optional.hpp>

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/slot/streamed.hpp"
#include "cocaine/rpc/upstream.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

namespace cocaine {

class state_machine_t;

/// An adapter for [Client -> Worker] message passing.
class client_rpc_dispatch_t:
    public dispatch<io::event_traits<io::app::enqueue>::dispatch_type>
{
public:
    /// Called on channel close.
    ///
    /// Guaranteed to be called once on either first error or close.
    typedef std::function<void(const std::error_code&)> callback_type;

private:
    typedef io::event_traits<io::app::enqueue>::dispatch_type incoming_tag;
    typedef io::event_traits<io::worker::rpc::invoke>::dispatch_type outcoming_tag;
    typedef io::protocol<incoming_tag>::scope protocol;

    enum class state_t {
        /// The dispatch is collecting messages into the queue.
        open,
        /// The dispatch is delegating incoming messages to the attached upstream.
        bound,
        /// The dispatch is closed normally or abnormally depending on `ec` variable.
        closed
    };

    /// Current state.
    state_t state;

    /// Upstream to the worker.
    streamed<std::string> stream;

    callback_type callback;

    /// Closed state error code.
    ///
    /// Non-null error code means that the dispatch is closed abnormally, i.e. client has been
    /// disconnected without closing its channels.
    std::error_code ec;

    std::mutex mutex;

public:
    explicit
    client_rpc_dispatch_t(const std::string& name);

    void
    attach(upstream<outcoming_tag> stream, callback_type callback);

    /// The client has been disconnected without closing its opened channels.
    ///
    /// In this case we should call close callback to prevend resource leak.
    virtual
    void
    discard(const std::error_code& ec) const;

    void
    discard(const std::error_code& ec);

private:
    void
    finalize();
};

} // namespace cocaine
