#include "cocaine/detail/service/node.v2/slave/state/terminating.hpp"

#include "cocaine/detail/service/node.v2/slave.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

terminating_t::terminating_t(std::shared_ptr<state_machine_t> slave_, std::unique_ptr<api::handle_t> handle_):
    slave(std::move(slave_)),
    handle(std::move(handle_)),
    timer(slave->loop)
{}

const char*
terminating_t::name() const noexcept {
    return "terminating";
}

void
terminating_t::terminate(const std::error_code& ec) {
    try {
        const std::size_t cancelled = timer.cancel();
        if (cancelled == 0) {
            COCAINE_LOG_WARNING(slave->log, "slave has been terminated, but the timeout has already expired");
        }
    } catch (...) {
    }

    slave->shutdown(ec);
}

void
terminating_t::start(unsigned long timeout) {
    COCAINE_LOG_DEBUG(slave->log, "slave is terminating, timeout: %.2f ms", timeout);

    timer.expires_from_now(boost::posix_time::milliseconds(timeout));
    timer.async_wait(std::bind(&terminating_t::on_timeout, shared_from_this(), ph::_1));
}

void
terminating_t::on_timeout(const std::error_code& ec) {
    if (!ec) {
        COCAINE_LOG_ERROR(slave->log, "unable to terminate slave: timeout");

        slave->shutdown(error::teminate_timeout);
    }
}
