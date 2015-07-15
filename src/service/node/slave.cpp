#include "cocaine/detail/service/node/slave.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node/slave/channel.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slave/fetcher.hpp"
#include "cocaine/detail/service/node/slave/state/active.hpp"
#include "cocaine/detail/service/node/slave/state/stopped.hpp"
#include "cocaine/detail/service/node/slave/state/handshaking.hpp"
#include "cocaine/detail/service/node/slave/state/spawning.hpp"
#include "cocaine/detail/service/node/slave/state/state.hpp"
#include "cocaine/detail/service/node/slave/state/terminating.hpp"
#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/util.hpp"

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

    // NOTE: Slave spawning involves starting timers and posting completion handlers callbacks with
    // the event loop.
    loop.post([=]() {
        // This call can perform state machine shutdowning on any error occurred.
        spawning->spawn(context.profile.timeout.spawn);
    });
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

slave::slave_stats_t
state_machine_t::stats() const {
    slave::slave_stats_t result { 0, 0, 0, 0, boost::none };

    data.channels.apply([&](const channels_map_t& channels) {
        for (const auto& channel : channels) {
            if (channel.second->send_closed()) {
                ++result.tx;
            }

            if (channel.second->recv_closed()) {
                ++result.rx;
            }
        }

        result.load = channels.size();
        result.total = counter;

        auto it = std::min_element(
            channels.begin(),
            channels.end(),
            [&](const std::pair<std::uint64_t, std::shared_ptr<channel_t>>& current,
                const std::pair<std::uint64_t, std::shared_ptr<channel_t>>& first) -> bool
        {
            return current.second->birthstamp() < first.second->birthstamp();
        });

        if (it != channels.end()) {
            result.age.reset(it->second->birthstamp());
        }
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
state_machine_t::inject(slave::channel_t& data, channel_handler handler) {
    const auto id = ++counter;

    auto channel = std::make_shared<channel_t>(
        id,
        data.event.birthstamp,
        std::bind(&state_machine_t::revoke, shared_from_this(), id, handler)
    );

    // W2C dispatch.
    auto dispatch = std::make_shared<const worker_rpc_dispatch_t>(
        data.downstream, [=](const std::error_code& ec) {
            if (ec) {
                channel->close_both();
            } else {
                channel->close_recv();
            }
        }
    );

    auto state = *this->state.synchronize();
    auto upstream = state->inject(dispatch);
    upstream->send<io::worker::rpc::invoke>(data.event.name);

    const auto load = this->data.channels.apply([&](channels_map_t& channels) -> std::uint64_t {
        channels[id] = channel;
        return channels.size();
    });

    COCAINE_LOG_DEBUG(log, "slave has started processing %d channel", id);
    COCAINE_LOG_TRACE(log, "slave has increased its load to %d", load)("channel", id);

    // C2W dispatch.
    data.dispatch->attach(upstream, [=](const std::error_code& ec) {
        if (ec) {
            channel->close_both();
        } else {
            channel->close_send();
        }
    });

    channel->watch();

    return id;
}

void
state_machine_t::seal() {
    auto state = *this->state.synchronize();

    state->seal();
}

void
state_machine_t::terminate(std::error_code ec) {
    BOOST_ASSERT(ec);

    if (closed.exchange(true)) {
        return;
    }

    COCAINE_LOG_TRACE(log, "slave state machine is terminating: %s", ec.message());

    auto state = *this->state.synchronize();
    state->terminate(ec);
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
    migrate(std::make_shared<stopped_t>(ec));

    fetcher->close();
    fetcher.reset();

    if (ec && ec != error::overseer_shutdowning) {
        dump();
    }

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

    // Terminate the state machine if the current state is sealing and there are no more channels
    // left.
    {
        auto state = *this->state.synchronize();
        if (state->sealing() && data.channels->empty()) {
            COCAINE_LOG_DEBUG(log, "sealing completed");
            state->terminate(error::slave_is_sealing);
        }
    }

    handler(id);
}

void
state_machine_t::dump() {
    if (lines.empty()) {
        COCAINE_LOG_WARNING(log, "slave has died in silence");
        return;
    }

    std::vector<std::string> dump;
    std::copy(lines.begin(), lines.end(), std::back_inserter(dump));

    const auto now = std::chrono::system_clock::now().time_since_epoch();

    const auto us = std::chrono::duration_cast<
        std::chrono::microseconds
    >(now).count();

    const auto key = format("%lld:%s", us, context.id);

    std::vector<std::string> indexes {
        context.manifest.name
    };

    std::time_t time = std::time(nullptr);
    char buf[64];
    if (auto len = std::strftime(buf, sizeof(buf), "cocaine-%Y-%m-%d", std::gmtime(&time))) {
        indexes.emplace_back(buf, len);
    }

    COCAINE_LOG_INFO(log, "slave is dumping output to 'crashlogs/%s' using [%s] indexes",
                     key, boost::join(indexes, ", "));

    try {
        api::storage(context.context, "core")->put("crashlogs", key, dump, indexes);
    } catch (const std::system_error& err) {
        COCAINE_LOG_WARNING(log, "slave is unable to save the crashlog: %s", err.what());
    }
}

slave_t::slave_t(slave_context context, asio::io_service& loop, cleanup_handler fn):
    ec(error::overseer_shutdowning),
    machine(state_machine_t::create(context, loop, fn))
{
    data.id = context.id;
    data.birthstamp = std::chrono::high_resolution_clock::now();
}

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

slave::slave_stats_t
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
slave_t::seal() {
    return machine->seal();
}

void
slave_t::terminate(std::error_code ec) {
    this->ec = std::move(ec);
}
