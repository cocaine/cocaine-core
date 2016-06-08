#pragma once

#include <metrics/tags.hpp>
#include <metrics/registry.hpp>

#include "cocaine/api/service.hpp"
#include "cocaine/context.hpp"
#include "cocaine/idl/runtime.hpp"
#include "cocaine/rpc/dispatch.hpp"

namespace cocaine {
namespace service {

class runtime_t : public api::service_t, public dispatch<io::runtime_tag> {
    metrics::registry_t& hub;

public:
    runtime_t(context_t& context,
              asio::io_service& asio,
              const std::string& name,
              const dynamic_t& args);

    auto prototype() const -> const io::basic_dispatch_t& {
        return *this;
    }

    /// Returns metrics dump.
    auto metrics() const -> dynamic_t;
};

}  // namespace service
}  // namespace cocaine
