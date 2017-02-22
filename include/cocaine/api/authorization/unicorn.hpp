#pragma once

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "cocaine/auth/uid.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/rpc/protocol.hpp"

namespace cocaine {
namespace api {
namespace authorization {

class unicorn_t {
public:
    typedef unicorn_t category_type;
    typedef std::function<void(std::error_code)> callback_type;

    virtual
    ~unicorn_t() = default;

    template<typename Event>
    auto
    verify(const std::string& path, const auth::identity_t& identity, callback_type callback) -> void {
        verify(io::event_traits<Event>::id, path, identity, std::move(callback));
    }

    virtual
    auto
    verify(std::size_t event, const std::string& path, const auth::identity_t& identity, callback_type callback)
        -> void = 0;
};

auto
unicorn(context_t& context, const std::string& service) -> std::shared_ptr<unicorn_t>;

} // namespace authorization
} // namespace api
} // namespace cocaine
