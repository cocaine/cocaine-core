#pragma once

#include "cocaine/hpack/header.hpp"

namespace cocaine {
namespace middleware {

/// Middleware that eats HPACK headers argument from the execution chain.
class drop_headers_t {
public:
    template<typename F, typename Event, typename... Args>
    auto
    operator()(F fn, Event, const hpack::headers_t&, Args&&... args) ->
        decltype(fn(std::forward<Args>(args)...))
    {
        return fn(std::forward<Args>(args)...);
    }
};

} // namespace middleware
} // namespace cocaine
