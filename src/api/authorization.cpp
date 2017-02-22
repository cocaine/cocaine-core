#include <memory>
#include <string>
#include <vector>

#include <boost/optional/optional.hpp>

#include "cocaine/api/authorization/event.hpp"
#include "cocaine/api/authorization/storage.hpp"
#include "cocaine/api/authorization/unicorn.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/repository.hpp"
#include "cocaine/repository/authorization.hpp"
#include "cocaine/rpc/protocol.hpp"

namespace cocaine {
namespace api {
namespace authorization {

auto
storage(context_t& context, const std::string& service) -> std::shared_ptr<storage_t> {
    const auto name = "storage";
    if (auto cfg = context.config().component_group("authorizations").get(name)) {
        return context.repository().get<storage_t>(cfg->type(), context, name, service, cfg->args());
    }

    throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
}

auto
unicorn(context_t& context, const std::string& service) -> std::shared_ptr<unicorn_t> {
    const auto name = "unicorn";
    if (auto cfg = context.config().component_group("authorizations").get(name)) {
        return context.repository().get<unicorn_t>(cfg->type(), context, name, service, cfg->args());
    }

    throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
}

} // namespace authorization
} // namespace api
} // namespace cocaine
