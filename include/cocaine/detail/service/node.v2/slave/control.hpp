#pragma once

#include <functional>

#include "cocaine/idl/rpc.hpp"
#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {

class state_machine_t;

/// Control channel for single slave.
///
/// NOTE: Worker should shut itself down after sending terminate message back (even if it initiates)
/// to the runtime.
class control_t:
    public dispatch<io::worker::control_tag>,
    public std::enable_shared_from_this<control_t>
{
    /// Attached slave.
    std::shared_ptr<state_machine_t> slave;

    /// Upstream to send messages to the worker.
    upstream<io::worker::control_tag> stream;

    /// Heartbeat timer.
    // TODO: Need synchronization.
    asio::deadline_timer timer;

    std::atomic<bool> closed;

public:
    control_t(std::shared_ptr<state_machine_t> slave, upstream<io::worker::control_tag> stream);

    virtual
    ~control_t();

    /// Starts health checking explicitly.
    void
    start();

    /// Sends terminate event to the slave.
    void
    terminate(const std::error_code& ec);

    /// Cancels all asynchronous operations on channel (e.g. timers).
    ///
    /// \note this method is required to be explicitly called on slave shutdown, because it breakes
    /// all cycle references inside the control channel.
    void
    cancel();

    virtual
    void
    discard(const std::error_code& ec) const ;

private:
    void
    on_heartbeat();

    void
    on_terminate(int ec, const std::string& reason);

    void
    breath();

    void
    on_timeout(const std::error_code& ec);
};

}
