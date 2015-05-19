#include "cocaine/detail/service/node_v2/slave/error.hpp"

#include <string>

using namespace cocaine;

class overseer_category_t:
    public std::error_category
{
public:
    const char*
    name() const noexcept {
        return "overseer";
    }

    std::string
    message(int ec) const {
        switch (static_cast<error::overseer_errors>(ec)) {
        case error::queue_is_full:
            return "queue is full";
        case error::pool_is_full:
            return "pool is full";
        default:
            return "unexpected overseer error";
        }
    }
};

class slave_category_t:
    public std::error_category
{
public:
    const char*
    name() const noexcept {
        return "slave";
    }

    std::string
    message(int ec) const {
        switch (static_cast<error::slave_errors>(ec)) {
        case error::spawn_timeout:
            return "timed out while spawning";
        case error::locator_not_found:
            return "locator not found";
        case error::activate_timeout:
            return "timed out while activating";
        case error::unknown_activate_error:
            return "unknown activate error";
        case error::teminate_timeout:
            return "timed out while terminating";
        case error::heartbeat_timeout:
            return "timed out while waiting for heartbeat";
        case error::invalid_state:
            return "invalid state";
        case error::conrol_ipc_error:
            return "unexpected control IPC error";
        case error::overseer_shutdowning:
            return "overseer is shutdowning";
        case error::committed_suicide:
            return "slave has committed suicide";
        case error::slave_idle:
            return "slave is idle";
        default:
            return "unexpected slave error";
        }
    }
};

const std::error_category&
error::overseer_category() {
    static overseer_category_t category;
    return category;
}

const std::error_category&
error::slave_category() {
    static slave_category_t category;
    return category;
}

std::error_code
error::make_error_code(overseer_errors err) {
    return std::error_code(static_cast<int>(err), error::overseer_category());
}

std::error_code
error::make_error_code(slave_errors err) {
    return std::error_code(static_cast<int>(err), error::slave_category());
}
