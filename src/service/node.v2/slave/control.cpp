#include "cocaine/detail/service/node.v2/slave/control.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

control_t::control_t(context_t& context, const std::string& name, const std::string& uuid) :
    dispatch<io::control_tag>(format("%s/control", name)),
    log(context.log(format("%s/control", name), blackhole::attribute::set_t({{ "uuid", uuid }})))
{
    on<io::control::heartbeat>([&](){
        COCAINE_LOG_DEBUG(log, "processing heartbeat message");
        // TODO: Reset heartbeat timer. slave->heartbeat();
        // TODO: Send heartbeat back.
    });

    // TODO: `on<io::control::terminate>();` - call detach from parent, send SIGTERM to worker,
    // wait for GRACEFUL_SHUTDOWN_TIMEOUT and send SIGKILL.
}

control_t::~control_t() {}

void control_t::discard(const std::error_code&) const {
    // Unix socket is destroyed some unexpected way, for example worker is down.
    // Detach slave from pool - `overseer->detach(uuid);`.
}
