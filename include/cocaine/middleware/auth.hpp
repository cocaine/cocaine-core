#pragma once

#include <string>
#include <system_error>
#include <vector>

#include <boost/variant/variant.hpp>

#include "cocaine/api/authentication.hpp"
#include "cocaine/auth/uid.hpp"
#include "cocaine/context.hpp"
#include "cocaine/hpack/header.hpp"

namespace cocaine {
namespace middleware {

class auth_t {
    std::shared_ptr<api::authentication_t> auth;

public:
    explicit
    auth_t(context_t& context, const std::string& service) :
        auth(api::authentication(context, "core", service))
    {}

    template<typename Event, typename F, typename... Args>
    auto
    operator()(F fn, Event, const hpack::headers_t& headers, Args&&... args) ->
        decltype(fn(headers, std::forward<Args>(args)...))
    {
        const auto identity = auth->identify(headers);

        if (auto ec = boost::get<std::error_code>(&identity)) {
            throw std::system_error(*ec, "permission denied");
        }

        return fn(headers, std::forward<Args>(args)..., boost::get<auth::identity_t>(identity));
    }
};

}  // namespace middleware
}  // namespace cocaine
