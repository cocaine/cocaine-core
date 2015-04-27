#include "cocaine/detail/service/node.v2/slave/control.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/tuple.hpp"

#include "cocaine/detail/service/node.v2/slave.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

control_t::control_t(std::shared_ptr<state_machine_t> slave, upstream<io::worker::control_tag> stream):
    dispatch<io::worker::control_tag>(format("%s/control", slave->context.manifest.name)),
    slave(std::move(slave)),
    stream(std::move(stream)),
    timer(this->slave->loop),
    closed(false)
{
    on<io::worker::heartbeat>(std::bind(&control_t::on_heartbeat, this));
    on<io::worker::terminate>(std::bind(&control_t::on_terminate, this, ph::_1, ph::_2));
}

control_t::~control_t() {
    COCAINE_LOG_TRACE(slave->log, "control channel has been destroyed");
}

void
control_t::start() {
    breath();
}

void control_t::terminate(const std::error_code& ec) {
    COCAINE_LOG_TRACE(slave->log, "sending terminate message");

    stream = stream.send<io::worker::terminate>(ec.value(), ec.message());
}

void
control_t::cancel() {
    closed.store(true);

    try {
        timer.cancel();
    } catch (...) {
        // We don't care.
    }
}

void
control_t::discard(const std::error_code& ec) const {
    if (ec && !closed) {
        COCAINE_LOG_DEBUG(slave->log, "control channel has been discarded: %s", ec.message());

        slave->close(error::conrol_ipc_error);
    }
}

void
control_t::on_heartbeat() {
    COCAINE_LOG_DEBUG(slave->log, "processing heartbeat message");

    breath();
}

void
control_t::on_terminate(int /*ec*/, const std::string& reason) {
    COCAINE_LOG_DEBUG(slave->log, "processing terminate message: %s", reason);

    // TODO: Check the error code to diverge between normal and abnormal slave shutdown.
    slave->close(error::committed_suicide);
}

void
control_t::breath() {
    COCAINE_LOG_TRACE(slave->log, "heartbeat timer has been restarted");

    timer.expires_from_now(boost::posix_time::milliseconds(slave->context.profile.timeout.heartbeat));
    timer.async_wait(std::bind(&control_t::on_timeout, shared_from_this(), ph::_1));
}

void
control_t::on_timeout(const std::error_code& ec) {
    // No error containing in error code indicates that the slave has failed to send heartbeat
    // message at least once in profile.timeout.heartbeat milliseconds.
    // In this case we should terminate it.
    if (ec) {
        COCAINE_LOG_TRACE(slave->log, "heartbeat timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_TRACE(slave->log, "heartbeat timer has called its completion handler: timeout");
        slave->close(error::heartbeat_timeout);
    }
}
