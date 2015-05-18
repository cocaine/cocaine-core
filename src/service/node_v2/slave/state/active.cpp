#include "cocaine/detail/service/node_v2/slave/state/active.hpp"

#include "cocaine/detail/service/node_v2/slave.hpp"
#include "cocaine/detail/service/node_v2/slave/control.hpp"
#include "cocaine/detail/service/node_v2/slave/state/terminating.hpp"

using namespace cocaine;

active_t::active_t(std::shared_ptr<state_machine_t> slave_,
                   std::unique_ptr<api::handle_t> handle_,
                   std::shared_ptr<session_t> session_,
                   std::shared_ptr<control_t> control_):
    slave(std::move(slave_)),
    handle(std::move(handle_)),
    session(std::move(session_)),
    control(std::move(control_))
{
    control->start();
}

active_t::~active_t() {
    if (control) {
        control->cancel();
    }

    if (session) {
        session->detach(asio::error::operation_aborted);
    }
}

const char*
active_t::name() const noexcept {
    return "active";
}

bool
active_t::active() const noexcept {
    return true;
}

io::upstream_ptr_t
active_t::inject(inject_dispatch_ptr_t dispatch) {
    BOOST_ASSERT(session);

    return session->fork(dispatch);
}

void
active_t::terminate(const std::error_code& ec) {
    auto terminating = std::make_shared<terminating_t>(
        slave, std::move(handle), std::move(control), std::move(session)
    );

    slave->migrate(terminating);

    terminating->start(slave->context.profile.timeout.terminate, ec);
}
