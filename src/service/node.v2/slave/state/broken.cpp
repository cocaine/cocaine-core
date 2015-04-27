#include "cocaine/detail/service/node.v2/slave/state/broken.hpp"

#include "cocaine/detail/service/node.v2/slave/error.hpp"

using namespace cocaine;

broken_t::broken_t(std::error_code ec):
    ec(std::move(ec))
{}

const char*
broken_t::name() const noexcept {
    switch (ec.value()) {
    case 0:
    case error::committed_suicide:
        return "closed";
    default:
        return "broken";
    }
}
