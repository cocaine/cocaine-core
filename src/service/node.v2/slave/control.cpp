#include "cocaine/detail/service/node.v2/slave/control.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

control_t::control_t(context_t& context, const std::string& name, const std::string& uuid) :
    dispatch<io::worker::control_tag>(format("%s/control", name)),
    log(context.log(format("%s/control", name), {{ "uuid", uuid }}))
{
    on<io::worker::heartbeat>([&](){
        COCAINE_LOG_DEBUG(log, "processing heartbeat message");

        // TODO: Reset heartbeat timer.
        // slave->heartbeat();

        // TODO: Send heartbeat back.
        // session->send<io::control::heartbeat>();
    });

    // TODO: Register terminate handler.
    // on<io::control::terminate>();
    // - Call detach from parent.
    // - Send SIGTERM to worker.
    // - Wait for timeout.terminate and send SIGKILL.
}

control_t::~control_t() {}

void control_t::discard(const std::error_code&) const {
    // Unix socket is destroyed some unexpected way, for example worker is down.
    // Detach slave from pool - `overseer->detach(uuid);`.
}
