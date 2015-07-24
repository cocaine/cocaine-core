#include "cocaine/detail/service/node/slave/state/spawning.hpp"

#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slave/state/handshaking.hpp"
#include "cocaine/detail/service/node/util.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

spawning_t::spawning_t(std::shared_ptr<state_machine_t> slave_) :
    slave(std::move(slave_)),
    timer(slave->loop)
{}

const char*
spawning_t::name() const noexcept {
    return "spawning";
}

void
spawning_t::cancel() {
    COCAINE_LOG_TRACE(slave->log, "processing spawn timer cancellation");

    try {
        const auto cancelled = timer.cancel();
        COCAINE_LOG_TRACE(slave->log, "processing spawn timer cancellation: done (%d cancelled)", cancelled);
    } catch (const std::system_error& err) {
        // If we are here, then something weird occurs with the timer.
        COCAINE_LOG_WARNING(slave->log, "unable to cancel spawn timer: %s", err.what());
    }
}

void
spawning_t::terminate(const std::error_code& ec) {
    cancel();
    slave->shutdown(ec);
}

void
spawning_t::spawn(unsigned long timeout) {
    COCAINE_LOG_DEBUG(slave->log, "slave is spawning using '%s', timeout: %.2f ms",
                      slave->context.manifest.executable, timeout);

    COCAINE_LOG_TRACE(slave->log, "locating the Locator endpoint list");
    auto locator = slave->context.context.locate("locator");
    if (!locator || locator->endpoints().empty()) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: failed to determine the Locator endpoint");

        slave->shutdown(error::locator_not_found);
        return;
    }

    // Fill Locator's endpoint list.
    std::ostringstream stream;
    const auto& endpoints = locator->endpoints();
    auto it = endpoints.begin();
    stream << *it;
    ++it;

    for (; it != endpoints.end(); ++it) {
        stream << "," << *it;
    }

    // Prepare command line arguments for worker instance.
    COCAINE_LOG_TRACE(slave->log, "preparing command line arguments");
    std::map<std::string, std::string> args;
    args["--uuid"]     = slave->context.id;
    args["--app"]      = slave->context.manifest.name;
    args["--endpoint"] = slave->context.manifest.endpoint;
    args["--locator"]  = stream.str();
    args["--protocol"] = std::to_string(io::protocol<io::worker_tag>::version::value);

    // Spawn a worker instance and start reading standard outputs of it.
    try {
        auto isolate = slave->context.context.get<api::isolate_t>(
            slave->context.profile.isolate.type,
            slave->context.context,
            slave->loop,
            slave->context.manifest.name,
            slave->context.profile.isolate.args
        );

        COCAINE_LOG_TRACE(slave->log, "spawning");

        timer.expires_from_now(boost::posix_time::milliseconds(timeout));
        timer.async_wait(std::bind(&spawning_t::on_timeout, shared_from_this(), ph::_1));

        handle = isolate->spawn(
            slave->context.manifest.executable,
            args,
            slave->context.manifest.environment
        );

        // Currently we spawn all slaves synchronously, but here is the right place to provide
        // a callback function to the Isolate.
        // NOTE: The callback must be called from the event loop thread, otherwise the behavior
        // is undefined.
        slave->loop.post(detail::move_handler(std::bind(
            &spawning_t::on_spawn, shared_from_this(), std::chrono::high_resolution_clock::now()
        )));
    } catch(const std::system_error& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: %s", err.code().message());

        slave->loop.post([=]() {
            slave->shutdown(err.code());
        });
    }
}

void
spawning_t::on_spawn(std::chrono::high_resolution_clock::time_point start) {
    std::error_code ec;
    const size_t cancelled = timer.cancel(ec);
    if (ec || cancelled == 0) {
        // If we are here, then the spawn timer has been triggered and the slave has been
        // shutdowned with a spawn timeout error.
        COCAINE_LOG_WARNING(slave->log, "slave has been spawned, but the timeout has already expired");
        return;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    COCAINE_LOG_DEBUG(slave->log, "slave has been spawned in %.2f ms",
        std::chrono::duration<float, std::chrono::milliseconds::period>(now - start).count());

    try {
        // May throw system error when failed to assign native descriptor to the fetcher.
        auto handshaking = std::make_shared<handshaking_t>(slave, std::move(handle));
        slave->migrate(handshaking);

        handshaking->start(slave->context.profile.timeout.handshake);
    } catch (const std::exception& err) {
        COCAINE_LOG_ERROR(slave->log, "unable to activate slave: %s", err.what());

        slave->shutdown(error::unknown_activate_error);
    }
}

void
spawning_t::on_timeout(const std::error_code& ec) {
    if (ec) {
        COCAINE_LOG_TRACE(slave->log, "spawn timer has called its completion handler: cancelled");
    } else {
        COCAINE_LOG_ERROR(slave->log, "unable to spawn slave: timeout");

        slave->shutdown(error::spawn_timeout);
    }
}
