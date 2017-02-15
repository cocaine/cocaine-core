#include "cocaine/api/controller.hpp"

#include <memory>
#include <string>
#include <vector>

#include <boost/optional/optional.hpp>

#include "cocaine/api/auth.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/repository.hpp"
#include "cocaine/repository/controller.hpp"
#include "cocaine/rpc/protocol.hpp"

namespace cocaine {
namespace api {
namespace controller {

auto
collection(context_t& context, const std::string& service) -> std::shared_ptr<collection_t> {
    const auto name = "collection";
    if (auto cfg = context.config().component_group("controllers").get(name)) {
        return context.repository().get<collection_t>(cfg->type(), context, name, service, cfg->args());
    }

    throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
}

} // namespace controller
} // namespace api
} // namespace cocaine
