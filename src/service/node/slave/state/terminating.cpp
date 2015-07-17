#include "cocaine/detail/service/node/slave/state/terminating.hpp"

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

terminating_t::terminating_t(std::shared_ptr<state_machine_t> slave_,
                             std::unique_ptr<api::handle_t> handle_,
                             std::shared_ptr<control_t> control_,
                             std::shared_ptr<session_t> session_):
    slave(std::move(slave_)),
    handle(std::move(handle_)),
    control(std::move(control_)),
    session(std::move(session_)),
    timer(slave->loop)
{}

terminating_t::~terminating_t() {
    control->cancel();
    session->detach(asio::error::operation_aborted);

    COCAINE_LOG_TRACE(slave->log, "state '%s' has been destroyed", name());
}

bool
terminating_t::terminating() const noexcept {
    return true;
}

const char*
terminating_t::name() const noexcept {
    return "terminating";
}

void
terminating_t::cancel() {
    try {
        timer.cancel();
    } catch (...) {
    }
}

void
terminating_t::terminate(const std::error_code& ec) {
    cancel();
    slave->shutdown(ec);
}

void
terminating_t::start(unsigned long timeout, const std::error_code& ec) {
    COCAINE_LOG_DEBUG(slave->log, "slave is terminating, timeout: %.2f ms", timeout);

    timer.expires_from_now(boost::posix_time::milliseconds(timeout));
    timer.async_wait(std::bind(&terminating_t::on_timeout, shared_from_this(), ph::_1));

    // The following operation may fail if the session is already disconnected. In this case a slave
    // shutdown operation will be triggered, which immediately stops the timer.
    control->terminate(ec);
}

void
terminating_t::on_timeout(const std::error_code& ec) {
    if (ec) {
        COCAINE_LOG_TRACE(slave->log, "termination timeout timer has been cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to terminate slave: timeout");

        slave->shutdown(error::teminate_timeout);
    }
}
