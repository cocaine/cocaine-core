#pragma once

#include <memory>
#include <string>

#include "cocaine/forwards.hpp"
#include "cocaine/rpc/protocol.hpp"

namespace cocaine {
namespace api {
namespace authorization {

/// Event-based authorization.
class event_t {
public:
    typedef event_t category_type;

    virtual
    ~event_t() = default;

    template<typename Event>
    auto
    verify(const auth::identity_t& identity) -> void {
        verify(Event::alias(), identity);
    }

    virtual
    auto
    verify(const std::string& event, const auth::identity_t& identity) -> void = 0;
};

auto
event(context_t& context, const std::string& service) -> std::shared_ptr<event_t>;

} // namespace authorization
} // namespace api
} // namespace cocaine
