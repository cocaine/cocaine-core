#pragma once

#include "cocaine/context.hpp"
#include "cocaine/idl/node.hpp"

namespace cocaine { namespace service { namespace node {

class app_state_t;

/// Represents a single application.
///
/// Starts TCP and UNIX servers.
class app_t {
    COCAINE_DECLARE_NONCOPYABLE(app_t)

    context_t& context;
    std::shared_ptr<app_state_t> state;

public:
    app_t(context_t& context, const std::string& manifest, const std::string& profile, deferred<void> deferred);
   ~app_t();

    std::string
    name() const;

    dynamic_t
    info() const;
};

}}} // namespace cocaine::service::node
