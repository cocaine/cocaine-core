#pragma once

#include <functional>

#include "cocaine/idl/rpc.hpp"
#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {

/// Control channel for single slave.
///
/// NOTE: Worker should shut itself down after sending terminate message back (even if it initiates)
/// to the runtime.
class control_t :
    public dispatch<io::control_tag>,
    public std::enable_shared_from_this<control_t>
{
    std::unique_ptr<logging::log_t> log;

    std::shared_ptr<session_t> session;

    std::function<void(std::error_code)> suicide;

public:
    control_t(context_t& context, const std::string& name, const std::string& uuid);

    ~control_t();

    virtual
    void
    discard(const std::error_code&) const override;

    void attach(std::shared_ptr<session_t> session);
    io::upstream_ptr_t inject(io::dispatch_ptr_t dispatch);
};

}
