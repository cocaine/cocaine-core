#include "cocaine/detail/service/node.v2/slave/error.hpp"

#include <string>

using namespace cocaine;

class slave_category_t : public std::error_category {
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
        case error::invalid_state:
            return "invalid state";
        default:
            return "unexpected slave error";
        }
    }
};

const std::error_category&
error::slave_category() {
    static slave_category_t category;
    return category;
}

std::error_code
error::make_error_code(slave_errors err) {
    return std::error_code(static_cast<int>(err), error::slave_category());
}
