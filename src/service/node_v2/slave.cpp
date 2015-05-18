#include "cocaine/detail/service/node_v2/slave.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node_v2/slave/control.hpp"
#include "cocaine/detail/service/node_v2/slave/fetcher.hpp"
#include "cocaine/detail/service/node_v2/slave/state/active.hpp"
#include "cocaine/detail/service/node_v2/slave/state/broken.hpp"
#include "cocaine/detail/service/node_v2/slave/state/handshaking.hpp"
#include "cocaine/detail/service/node_v2/slave/state/spawning.hpp"
#include "cocaine/detail/service/node_v2/slave/state/state.hpp"
#include "cocaine/detail/service/node_v2/slave/state/terminating.hpp"
#include "cocaine/detail/service/node_v2/dispatch/worker.hpp"
#include "cocaine/detail/service/node_v2/util.hpp"

namespace ph = std::placeholders;

using namespace cocaine;

std::shared_ptr<state_machine_t>
state_machine_t::create(slave_context context, asio::io_service& loop, cleanup_handler cleanup) {
    auto machine = std::make_shared<state_machine_t>(lock_t(), context, loop, cleanup);
    machine->start();

    return machine;
}

state_machine_t::state_machine_t(lock_t, slave_context context, asio::io_service& loop, cleanup_handler cleanup):
    log(context.context.log(format("%s/slave", context.manifest.name), {{ "uuid", context.id }})),
    context(context),
    loop(loop),
    closed(false),
    cleanup(std::move(cleanup)),
    lines(context.profile.crashlog_limit),
    shutdowned(false)
{
    COCAINE_LOG_TRACE(log, "slave state machine has been initialized");
}

state_machine_t::~state_machine_t() {
    COCAINE_LOG_TRACE(log, "slave state machine has been destroyed");
}

void
state_machine_t::start() {
    BOOST_ASSERT(*state.synchronize() == nullptr);

    COCAINE_LOG_TRACE(log, "slave state machine is starting");

    fetcher = std::make_shared<fetcher_t>(shared_from_this());

    auto spawning = std::make_shared<spawning_t>(shared_from_this());
    migrate(spawning);

    // This call can perform state machine shutdowning on any error occurred.
    spawning->spawn(context.profile.timeout.spawn);
}

bool
state_machine_t::active() const noexcept {
    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->active();
}

std::shared_ptr<control_t>
state_machine_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->activate(std::move(session), std::move(stream));
}

io::upstream_ptr_t
state_machine_t::inject(inject_dispatch_ptr_t dispatch) {
    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->inject(std::move(dispatch));
}

void
state_machine_t::terminate(std::error_code ec) {
    BOOST_ASSERT(ec);

    if (closed.exchange(true)) {
        return;
    }

    COCAINE_LOG_TRACE(log, "slave state machine is terminating: %s", ec.message());

    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->terminate(ec);
}

void
state_machine_t::output(const char* data, size_t size) {
    splitter.consume(std::string(data, size));
    while (auto line = splitter.next()) {
        lines.push_back(*line);

        if (context.profile.log_output) {
            COCAINE_LOG_DEBUG(log, "slave's output: `%s`", *line);
        }
    }
}

void
state_machine_t::migrate(std::shared_ptr<state_t> desired) {
    BOOST_ASSERT(desired);

    state.apply([=](std::shared_ptr<state_t>& state){
        COCAINE_LOG_DEBUG(log, "slave has changed its state from '%s' to '%s'",
            state ? state->name() : "null", desired->name()
        );

        state = std::move(desired);
    });
}

void
state_machine_t::shutdown(std::error_code ec) {
    if (shutdowned.exchange(true)) {
        return;
    }

    COCAINE_LOG_TRACE(log, "slave is shutdowning: %s", ec.message());

    auto state = *this->state.synchronize();
    state->cancel();
    migrate(std::make_shared<broken_t>(ec));

    fetcher->close();
    fetcher.reset();

    // Check if the slave has been terminated externally. If so, do not call the cleanup callback.
    if (closed) {
        return;
    }

    try {
        cleanup(ec);
    } catch (...) {
        // Just eat an exception, we don't care why the cleanup handler failed to do its job.
    }
}

slave_t::slave_t(slave_context context, asio::io_service& loop, cleanup_handler fn):
    ec(error::overseer_shutdowning),
    machine(state_machine_t::create(context, loop, fn))
{}

slave_t::~slave_t() {
    // This condition is required, because the class itself is movable.
    if (machine) {
        machine->terminate(std::move(ec));
    }
}

bool
slave_t::active() const noexcept {
    BOOST_ASSERT(machine);

    return machine->active();
}

std::shared_ptr<control_t>
slave_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    BOOST_ASSERT(machine);

    return machine->activate(std::move(session), std::move(stream));
}

io::upstream_ptr_t
slave_t::inject(inject_dispatch_ptr_t dispatch) {
    BOOST_ASSERT(machine);

    return machine->inject(std::move(dispatch));
}

void
slave_t::terminate(std::error_code ec) {
    this->ec = std::move(ec);
}
