#pragma once

#include <string>
#include <system_error>
#include <vector>

#include <boost/variant/variant.hpp>

#include "cocaine/api/auth.hpp"
#include "cocaine/context.hpp"
#include "cocaine/hpack/header.hpp"
#include "cocaine/hpack/header_definitions.hpp"

namespace cocaine {
namespace middleware {

class auth_t {
    std::shared_ptr<api::auth_t> auth;

public:
    explicit
    auth_t(context_t& context, const std::string& service) :
        auth(api::auth(context, "core", service))
    {}

    template<typename Event, typename F, typename... Args>
    auto
    operator()(F fn, Event, const hpack::headers_t& headers, Args&&... args) ->
        decltype(fn(headers, std::forward<Args>(args)...))
    {
        // Even if there is no credentials provided some authorization components may allow access.
        std::string credentials;
        if (auto header = hpack::header::find_first<hpack::headers::authorization<>>(headers)) {
            credentials = header->value();
        }

        const auto perm = auth->check_permissions<Event>(credentials);

        if (auto ec = boost::get<std::error_code>(&perm)) {
            throw std::system_error(*ec, "permission denied");
        }

        return fn(headers, std::forward<Args>(args)..., boost::get<api::auth_t::allow_t>(perm).uids);
    }
};

}  // namespace middleware
}  // namespace cocaine
