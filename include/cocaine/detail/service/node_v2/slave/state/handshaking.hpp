#pragma once

#include "state.hpp"

namespace cocaine {

namespace api { class handle_t; }

class state_machine_t;

class handshaking_t:
    public state_t,
    public std::enable_shared_from_this<handshaking_t>
{
    std::shared_ptr<state_machine_t> slave;

    synchronized<asio::deadline_timer> timer;
    std::unique_ptr<api::handle_t> handle;

    std::chrono::high_resolution_clock::time_point birthtime;

public:
    handshaking_t(std::shared_ptr<state_machine_t> slave, std::unique_ptr<api::handle_t> handle);

    virtual
    const char*
    name() const noexcept;

    virtual
    void
    terminate(const std::error_code& ec);

    /// Activates the slave by transferring it to the active state using given session and control
    /// channel.
    ///
    /// \threadsafe
    virtual
    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream);

    void
    start(unsigned long timeout);

private:
    void
    on_timeout(const std::error_code& ec);
};

}
