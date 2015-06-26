#include "cocaine/detail/service/node/slave/state/state.hpp"

#include "cocaine/detail/service/node/slave/error.hpp"

using namespace cocaine;

void
state_t::cancel() {}

bool
state_t::active() const noexcept {
    return false;
}

bool
state_t::sealing() const noexcept {
    return false;
}

std::shared_ptr<control_t>
state_t::activate(std::shared_ptr<session_t> /*session*/, upstream<io::worker::control_tag> /*stream*/) {
    throw_invalid_state();
}

io::upstream_ptr_t
state_t::inject(inject_dispatch_ptr_t /*dispatch*/) {
    throw_invalid_state();
}

void
state_t::seal() {
    throw_invalid_state();
}

void
state_t::terminate(const std::error_code& /*ec*/) {}

void
state_t::throw_invalid_state() {
    throw std::system_error(error::invalid_state, format("invalid state (%s)", name()));
}
