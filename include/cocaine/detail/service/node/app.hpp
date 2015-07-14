#pragma once

#include <string>

#include "cocaine/common.hpp"
#include "cocaine/rpc/slot/deferred.hpp"

namespace cocaine {
    namespace service {
        namespace node {
            class app_state_t;
        } // namespace node
    } // namespace service
} // namespace cocaine

namespace cocaine { namespace service { namespace node {

/// Represents a single application.
///
/// Starts TCP and UNIX servers.
class app_t {
    COCAINE_DECLARE_NONCOPYABLE(app_t)

public:
    // WARNING: Unstable - just added.
    enum class info_policy_t {
        brief,
        verbose
    };

private:
    std::shared_ptr<app_state_t> state;

public:
    app_t(context_t& context, const std::string& manifest, const std::string& profile, deferred<void> deferred);
   ~app_t();

    std::string
    name() const;

    // WARNING: Unstable - just added.
    dynamic_t
    info(info_policy_t policy = info_policy_t::brief) const;
};

}}} // namespace cocaine::service::node
