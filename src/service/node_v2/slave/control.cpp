#include "cocaine/detail/service/node_v2/slave/control.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/tuple.hpp"

#include "cocaine/detail/service/node_v2/slave.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

control_t::control_t(std::shared_ptr<state_machine_t> slave_, upstream<io::worker::control_tag> stream_):
    dispatch<io::worker::control_tag>(format("%s/control", slave_->context.manifest.name)),
    slave(std::move(slave_)),
    stream(std::move(stream_)),
    timer(slave->loop),
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
    COCAINE_LOG_TRACE(slave->log, "heartbeat timer has been started");

    timer.expires_from_now(boost::posix_time::milliseconds(slave->context.profile.timeout.heartbeat));
    timer.async_wait(std::bind(&control_t::on_timeout, shared_from_this(), ph::_1));
}

void
control_t::terminate(const std::error_code& ec) {
    BOOST_ASSERT(ec);

    COCAINE_LOG_TRACE(slave->log, "sending terminate message");

    try {
        stream = stream.send<io::worker::terminate>(ec.value(), ec.message());
    } catch (const cocaine::error_t& err) {
        COCAINE_LOG_WARNING(slave->log, "failed to send terminate message: %s", err.what());
        slave->shutdown(error::conrol_ipc_error);
    }
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

        slave->shutdown(error::conrol_ipc_error);
    }
}

void
control_t::on_heartbeat() {
    COCAINE_LOG_DEBUG(slave->log, "processing heartbeat message");

    if (closed) {
        COCAINE_LOG_TRACE(slave->log, "heartbeat message has been dropped: control is closed");
    } else {
        COCAINE_LOG_TRACE(slave->log, "heartbeat timer has been restarted");

        timer.expires_from_now(boost::posix_time::milliseconds(slave->context.profile.timeout.heartbeat));
        timer.async_wait(std::bind(&control_t::on_timeout, shared_from_this(), ph::_1));
    }
}

void
control_t::on_terminate(int /*ec*/, const std::string& reason) {
    COCAINE_LOG_DEBUG(slave->log, "processing terminate message: %s", reason);

    // TODO: Check the error code to diverge between normal and abnormal slave shutdown. More will
    // be implemented after error_categories come.
    slave->shutdown(error::committed_suicide);
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
        slave->shutdown(error::heartbeat_timeout);
    }
}
