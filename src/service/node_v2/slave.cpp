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
#include "cocaine/detail/service/node_v2/dispatch/client.hpp"
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
    shutdowned(false),
    counter{}
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

std::uint64_t
state_machine_t::load() const {
    return load_->size();
}

std::shared_ptr<control_t>
state_machine_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->activate(std::move(session), std::move(stream));
}

std::uint64_t
state_machine_t::inject(slave::channel_t& channel, channel_handler handler) {
    auto state = *this->state.synchronize();

    const auto id = ++counter;
    auto rxcb = std::bind(&state_machine_t::on_rx_channel_close, shared_from_this(), id);

    // W2C dispatch.
    auto dispatch = std::make_shared<const worker_client_dispatch_t>(
        channel.upstream,
        [=](std::exception*) {
            // Error here usually indicates about client disconnection.
            // TODO: If the client disappears we can send an error to the worker (ECONNRESET).
            // TODO: Here this callback can be called multiple times (?), which leads to the UB.
            rxcb();
        }
    );

    auto upstream = state->inject(dispatch);
    upstream->send<io::worker::rpc::invoke>(channel.event);

    const auto load = load_.apply([&](load_map_t& load) -> std::uint64_t {
        load[id] = { side_t::both, handler };
        return load.size();
    });

    COCAINE_LOG_DEBUG(log, "slave has started processing %d channel", id);
    COCAINE_LOG_TRACE(log, "slave has increased its load to %d (channel: %d)", load, id);

    // C2W dispatch.
    channel.dispatch->attach(
        std::move(upstream),
        std::bind(&state_machine_t::on_tx_channel_close, shared_from_this(), id)
    );

    return id;
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

void
state_machine_t::on_tx_channel_close(std::uint64_t id) {
    on_channel_close(id, side_t::tx);
}

void
state_machine_t::on_rx_channel_close(std::uint64_t id) {
    on_channel_close(id, side_t::rx);
}

void
state_machine_t::on_channel_close(std::uint64_t id, side_t side) {
    auto handler = load_.apply([&](load_map_t& load) -> channel_handler {
        auto it = load.find(id);

        // Ensure that we call this callback once.
        if (it == load.end()) {
            COCAINE_LOG_WARNING(log, "fuck you: %s %d", side == side_t::tx ? "tx" : "rx", id);
            return [](std::uint64_t){};
        }
        COCAINE_LOG_TRACE(log, "closing %s side of %d channel", side == side_t::tx ? "tx" : "rx", id);

        it->second.side &= ~side;

        if (it->second.side == side_t::none) {
            auto handler = it->second.handler;
            load.erase(it);

            COCAINE_LOG_TRACE(log, "slave has decreased its load to %d (channel: %d)", load.size(), id);
            COCAINE_LOG_DEBUG(log, "slave has closed its %d channel", id);

            return handler;
        }

        return [](std::uint64_t){};
    });

    handler(id);
}

slave_t::slave_t(slave_context context, asio::io_service& loop, cleanup_handler fn):
    ec(error::overseer_shutdowning),
    data{ context.id, std::chrono::high_resolution_clock::now() },
    machine(state_machine_t::create(context, loop, fn))
{}

slave_t::~slave_t() {
    // This condition is required, because the class itself is movable.
    if (machine) {
        machine->terminate(std::move(ec));
    }
}

const std::string&
slave_t::id() const noexcept {
    return data.id;
}

long long
slave_t::uptime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - data.birthstamp
    ).count();
}

std::uint64_t
slave_t::load() const {
    BOOST_ASSERT(machine);

    return machine->load();
}

slave_t::channel_stats_t
slave_t::stats() const {
    auto load = *machine->load_.synchronize();

    channel_stats_t stats {};
    stats.load = load.size();
    for (auto& it : load) {
        if ((it.second.side & 0x01) == 0x01) {
            stats.tx++;
        }

        if ((it.second.side & 0x02) == 0x02) {
            stats.rx++;
        }
    }

    return stats;
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

std::uint64_t
slave_t::inject(slave::channel_t& channel, state_machine_t::channel_handler handler) {
    BOOST_ASSERT(machine);

    return machine->inject(channel, handler);
}

void
slave_t::terminate(std::error_code ec) {
    this->ec = std::move(ec);
}
