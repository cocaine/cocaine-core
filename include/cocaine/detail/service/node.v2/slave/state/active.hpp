#pragma once

#include <memory>

#include "state.hpp"

namespace cocaine {

namespace api { class handle_t; }

class state_machine_t;

class active_t:
    public state_t,
    public std::enable_shared_from_this<active_t>
{
    std::shared_ptr<state_machine_t> slave;
    std::unique_ptr<api::handle_t> handle;
    std::shared_ptr<session_t> session;
    std::shared_ptr<control_t> control;

public:
    active_t(std::shared_ptr<state_machine_t> slave,
             std::unique_ptr<api::handle_t> handle,
             std::shared_ptr<session_t> session,
             std::shared_ptr<control_t> control);

    ~active_t();

    virtual
    const char*
    name() const noexcept;

    virtual
    bool
    active() const noexcept;

    virtual
    io::upstream_ptr_t
    inject(inject_dispatch_ptr_t dispatch);

    /// Migrates the current state to the terminating one.
    ///
    /// This is achieved by sending the terminate event to the slave and waiting for its response
    /// for a specified amount of time (timeout.terminate).
    ///
    /// The slave also becomes inactive (i.e. unable to handle new channels).
    ///
    /// If the slave is unable to ack the termination event it will be considered as broken and
    /// should be removed from the pool.
    ///
    /// \warning this call invalidates the current object.
    virtual
    void
    terminate(const std::error_code& ec);
};

} // namespace cocaine
