#include "cocaine/detail/service/node.v2/drone.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

std::shared_ptr<drone_t>
drone_t::make(context_t& context, drone_data data, std::shared_ptr<asio::io_service> loop) {
    auto drone = std::make_shared<drone_t>(context, data, loop);
    drone->watch();
    return drone;
}

drone_t::drone_t(context_t& context, drone_data data, std::shared_ptr<asio::io_service> loop) :
    d(std::move(data)),
    log(context.log(format("drone/%s", d.manifest.name), blackhole::attribute::set_t({{ "drone", d.id }}))),
    watcher(*loop)
{
    COCAINE_LOG_DEBUG(log, "slave is activating, timeout: %.02f seconds", d.profile.startup_timeout);
//    heartbeat_timer.expires_from_now(boost::posix_time::seconds(m_profile.startup_timeout));
//    heartbeat_timer.async_wait(std::bind(&slave_t::on_timeout, shared_from_this(), ph::_1));

    COCAINE_LOG_DEBUG(log, "slave is spawning using '%s'", d.manifest.executable);

    // Prepare command line arguments for worker instance.

    // TODO: Hardcoded locator name.
    auto locator = context.locate("locator");

    std::map<std::string, std::string> args;
    args["--uuid"]     = d.id;
    args["--app"]      = d.manifest.name;
    args["--endpoint"] = d.manifest.endpoint;
    // TODO: It's better to send resolver endpoints list.

    if (!locator) {
        // TODO: What if there are no locator?
        COCAINE_LOG_ERROR(log, "unable to determine locator endpoint");
        throw std::runtime_error("no locator provided");
    }

    args["--locator"]  = cocaine::format("%s:%d", context.config.network.hostname, locator->endpoints().front().port());

    // Spawn a worker instance and start reading standard outputs of it.
    try {
        // TODO: What if there are no isolate?
        auto isolate = context.get<api::isolate_t>(
            d.profile.isolate.type,
            context,
            d.manifest.name,
            d.profile.isolate.args
        );

        handle = isolate->spawn(d.manifest.executable, args, d.manifest.environment);
        watcher.assign(handle->stdout());
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(log, "unable to spawn more slaves: [%d] %s", e.code().value(), e.code().message());
        throw;
    }
}

void drone_t::watch() {
    // TODO: Log.
    watcher.async_read_some(asio::buffer(buffer.data(), buffer.size()), std::bind(&drone_t::on_watch, shared_from_this(), ph::_1, ph::_2));
}

void
drone_t::on_watch(const std::error_code& ec, size_t len) {
    if (ec) {
        switch (ec.value()) {
        case asio::error::operation_aborted:
            return;
        case asio::error::eof:
            COCAINE_LOG_DEBUG(log, "slave has closed its output");
            break;
        default:
            COCAINE_LOG_WARNING(log, "slave has failed to read output: %s", ec.message());
            // TODO: Something abnormal happened. Maybe terminate itself?
        }

        return;
    }

    COCAINE_LOG_DEBUG(log, "received %d bytes of output", len);

    d.output(std::string(buffer.data(), len));

    watch();
}

drone_t::~drone_t() {
    COCAINE_LOG_DEBUG(log, "processing drone termination");
    handle->terminate();
}

void drone_t::terminate() {
    // Send terminate message.
    watcher.cancel();
}
