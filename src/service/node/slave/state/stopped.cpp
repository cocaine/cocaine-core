#include "cocaine/detail/service/node/slave/state/stopped.hpp"

#include "cocaine/detail/service/node/slave/error.hpp"

using namespace cocaine;

stopped_t::stopped_t(std::error_code ec):
    ec(std::move(ec))
{}

void
stopped_t::cancel() {}

const char*
stopped_t::name() const noexcept {
    switch (ec.value()) {
    case 0:
    case error::committed_suicide:
        return "closed";
    default:
        return "broken";
    }
}
