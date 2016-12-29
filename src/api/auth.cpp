#include "cocaine/api/auth.hpp"

#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/repository.hpp"

#include <boost/optional/optional.hpp>

#include "cocaine/repository/auth.hpp"

namespace cocaine {
namespace api {

auto
auth(context_t& context, const std::string& name, const std::string& service) ->
    std::shared_ptr<auth_t>
{
    if (auto cfg = context.config().component_group("authorizations").get(name)) {
        return context.repository().get<auth_t>(cfg->type(), context, name, service, cfg->args());
    }

    throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
}

}  // namespace api
}  // namespace cocaine
