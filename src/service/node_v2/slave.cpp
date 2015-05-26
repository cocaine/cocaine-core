#include "cocaine/detail/service/node_v2/slave.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node_v2/slave/channel.hpp"
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
    return data.channels->size();
}

slave::channel_stats_t
state_machine_t::stats() const {
    slave::channel_stats_t result {};

    data.channels.apply([&](const channels_map_t& channels) {
        for (const auto& channel : channels) {
            if ((channel.second->state() & channel_t::state_t::rx) == channel_t::state_t::rx) {
                ++result.rx;
            }

            if ((channel.second->state() & channel_t::state_t::tx) == channel_t::state_t::tx) {
                ++result.tx;
            }
        }

        result.load = channels.size();
        result.total = counter;
    });

    return result;
}

std::shared_ptr<control_t>
state_machine_t::activate(std::shared_ptr<session_t> session, upstream<io::worker::control_tag> stream) {
    auto state = *this->state.synchronize();
    BOOST_ASSERT(state);

    return state->activate(std::move(session), std::move(stream));
}

std::uint64_t
state_machine_t::inject(slave::channel_t& channel_data, channel_handler handler) {
    auto state = *this->state.synchronize();

    const auto id = ++counter;

    // keep slave callback to erase itself.
    // keep id.
    // tracks both callbacks and error cases.
    // may throw.
    auto channel = std::make_shared<channel_t>(
        id,
        std::bind(&state_machine_t::revoke, shared_from_this(), id, handler)
    );

    // W2C dispatch.
    // may throw - it's ok.
    auto dispatch = std::make_shared<const worker_rpc_dispatch_t>(
        channel_data.downstream, [=](const std::error_code& ec) {
            channel->close(channel_t::rx, ec);
        }
    );

    // may throw both.
    auto upstream = state->inject(dispatch);
    upstream->send<io::worker::rpc::invoke>(channel_data.event);

    // register channel.
    // nothrow.
    const auto load = data.channels.apply([&](channels_map_t& channels) -> std::uint64_t {
        channels[id] = channel;
        return channels.size();
    });

    COCAINE_LOG_DEBUG(log, "slave has started processing %d channel", id);
    COCAINE_LOG_TRACE(log, "slave has increased its load to %d", load)("channel", id);

    // C2W dispatch.
    channel_data.dispatch->attach(upstream, [=](const std::error_code& ec) {
        channel->close(channel_t::tx, ec);
    });

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
state_machine_t::revoke(std::uint64_t id, channel_handler handler) {
    const auto load = data.channels.apply([&](channels_map_t& channels) -> std::uint64_t {
        channels.erase(id);
        return channels.size();
    });

    COCAINE_LOG_TRACE(log, "slave has decreased its load to %d", load)("channel", id);
    COCAINE_LOG_DEBUG(log, "slave has closed its %d channel", id);

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

slave::channel_stats_t
slave_t::stats() const {
    BOOST_ASSERT(machine);
    return machine->stats();
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
