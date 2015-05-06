#include "cocaine/detail/service/node_v2/slave/state/handshaking.hpp"

#include "cocaine/detail/service/node_v2/slave.hpp"
#include "cocaine/detail/service/node_v2/slave/control.hpp"
#include "cocaine/detail/service/node_v2/slave/fetcher.hpp"
#include "cocaine/detail/service/node_v2/slave/state/active.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

handshaking_t::handshaking_t(std::shared_ptr<state_machine_t> slave_, std::unique_ptr<api::handle_t> handle_):
    slave(std::move(slave_)),
    timer(slave->loop),
    handle(std::move(handle_)),
    birthtime(std::chrono::high_resolution_clock::now())
{
    slave->fetcher->assign(handle->stdout());
}

const char*
handshaking_t::name() const noexcept {
    return "handshaking";
}

void
handshaking_t::terminate(const std::error_code& ec) {
    try {
        timer->cancel();
    } catch (...) {
        // We don't care.
    }

    slave->shutdown(ec);
}

std::shared_ptr<control_t>
handshaking_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    std::error_code ec;

    const size_t cancelled = timer->cancel(ec);
    if (ec || cancelled == 0) {
        COCAINE_LOG_WARNING(slave->log, "slave has been activated, but the timeout has already expired");
        return nullptr;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    COCAINE_LOG_DEBUG(slave->log, "slave has been activated in %.2f ms",
        std::chrono::duration<float, std::chrono::milliseconds::period>(now - birthtime).count());

    try {
        auto control = std::make_shared<control_t>(slave, std::move(stream));
        auto active = std::make_shared<active_t>(slave, std::move(handle), std::move(session), control);
        slave->migrate(active);

        return control;
    } catch (const std::exception& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: %s", err.what());

        slave->shutdown(error::unknown_activate_error);
    }

    return nullptr;
}

void
handshaking_t::start(unsigned long timeout) {
    COCAINE_LOG_DEBUG(slave->log, "slave is waiting for handshake, timeout: %.2f ms", timeout);

    timer.apply([&](asio::deadline_timer& timer){
        timer.expires_from_now(boost::posix_time::milliseconds(timeout));
        timer.async_wait(std::bind(&handshaking_t::on_timeout, shared_from_this(), ph::_1));
    });
}

void
handshaking_t::on_timeout(const std::error_code& ec) {
    if (!ec) {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: timeout");

        slave->shutdown(error::activate_timeout);
    }
}
