#pragma once

#include <memory>

#include "cocaine/idl/rpc.hpp"
#include "cocaine/rpc/upstream.hpp"

namespace cocaine {

typedef std::shared_ptr<
    const dispatch<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
> inject_dispatch_ptr_t;

class control_t;
class session_t;

class state_t {
public:
    virtual
    ~state_t() {}

    virtual
    const char*
    name() const noexcept = 0;

    /// Cancels all pending asynchronous operations.
    ///
    /// Should be invoked on slave shutdown, indicating that the current state should cancel all
    /// its asynchronous operations to break cyclic references.
    virtual
    void
    cancel();

    /// True for slaves that can handle new channels.
    virtual
    bool
    active() const noexcept;

    virtual
    std::shared_ptr<control_t>
    activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream);

    virtual
    io::upstream_ptr_t
    inject(inject_dispatch_ptr_t dispatch);

    /// Terminates the slave with the given error code.
    virtual
    void
    terminate(const std::error_code& ec);

private:
    void
    __attribute__((noreturn))
    throw_invalid_state();
};

}
