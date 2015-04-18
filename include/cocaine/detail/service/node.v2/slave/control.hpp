#pragma once

#include <functional>

#include "cocaine/idl/rpc.hpp"
#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/detail/service/node.v2/slave.hpp"

namespace cocaine {

/// Control channel for single slave.
///
/// NOTE: Worker should shut itself down after sending terminate message back (even if it initiates)
/// to the runtime.
class control_t :
    public dispatch<io::worker::control_tag>,
    public std::enable_shared_from_this<control_t>
{
    const std::unique_ptr<logging::log_t> log;

    std::function<void(std::error_code)> suicide;

    upstream<io::worker::control_tag> stream;

public:
    control_t(cocaine::slave_context ctx, asio::io_service& loop, upstream<io::worker::control_tag> stream);

    ~control_t();

    virtual
    void
    discard(const std::error_code&) const override;
};

}
