#pragma once

#include <functional>
#include <system_error>

#include "cocaine/idl/context.hpp"
#include "cocaine/signal.hpp"

namespace cocaine {

struct propagate_t {
    retroactive_signal<io::context_tag>& hub;
    signal::handler_base_t& handler;
    std::function<void()> callback;

    auto
    operator()(const std::error_code& ec, int signum, const siginfo_t& info) -> void {
        if(ec == std::errc::operation_canceled) {
            return;
        }

        if (callback) {
            callback();
        }

        hub.invoke<io::context::os_signal>(signum, info);
        handler.async_wait(signum, *this);
    }
};

}  // namespace cocaine
