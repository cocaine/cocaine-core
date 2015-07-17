#include "cocaine/detail/service/node/slave/state/sealing.hpp"

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/state/terminating.hpp"

namespace ph = std::placeholders;

using namespace cocaine::service::node::slave::state;

sealing_t::sealing_t(std::shared_ptr<cocaine::state_machine_t> slave_,
                     std::unique_ptr<cocaine::api::handle_t> handle_,
                     std::shared_ptr<cocaine::control_t> control_,
                     std::shared_ptr<cocaine::session_t> session_):
    slave(std::move(slave_)),
    handle(std::move(handle_)),
    control(std::move(control_)),
    session(std::move(session_)),
    timer(slave->loop)
{}

void
sealing_t::cancel() {
    COCAINE_LOG_TRACE(slave->log, "processing seal timer cancellation");

    try {
        const auto cancelled = timer.cancel();
        COCAINE_LOG_TRACE(slave->log, "processing seal timer cancellation: done (%d cancelled)", cancelled);
    } catch (const std::system_error& err) {
        COCAINE_LOG_WARNING(slave->log, "unable to cancel seal timer: %s", err.what());
    }
}

void
sealing_t::start(unsigned long timeout) {
    if (slave->data.channels->empty()) {
        terminate(error::slave_is_sealing);
        return;
    }

    COCAINE_LOG_DEBUG(slave->log, "slave is sealing, timeout: %.2f ms", timeout);

    timer.expires_from_now(boost::posix_time::milliseconds(timeout));
    timer.async_wait(std::bind(&sealing_t::on_timeout, shared_from_this(), ph::_1));
}

void
sealing_t::terminate(const std::error_code& ec) {
    COCAINE_LOG_DEBUG(slave->log, "slave is terminating after been sealed: %s", ec.message());

    cancel();

    auto terminating = std::make_shared<terminating_t>(
        slave, std::move(handle), std::move(control), std::move(session)
    );

    slave->migrate(terminating);

    terminating->start(slave->context.profile.timeout.terminate, ec);
}

void
sealing_t::on_timeout(const std::error_code& ec) {
    if (ec) {
        COCAINE_LOG_TRACE(slave->log, "seal timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to seal slave: timeout");

        terminate(error::seal_timeout);
    }
}
