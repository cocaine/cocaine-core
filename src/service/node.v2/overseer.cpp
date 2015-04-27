#include "cocaine/detail/service/node.v2/overseer.hpp"

#include <blackhole/scoped_attributes.hpp>

#include "cocaine/context.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node.v2/balancing/base.hpp"
#include "cocaine/detail/service/node.v2/balancing/null.hpp"
#include "cocaine/detail/service/node.v2/dispatch/client.hpp"
#include "cocaine/detail/service/node.v2/dispatch/handshaker.hpp"
#include "cocaine/detail/service/node.v2/dispatch/worker.hpp"
#include "cocaine/detail/service/node.v2/slave/control.hpp"
#include "cocaine/detail/service/node.v2/slot.hpp"

using namespace cocaine;

using namespace blackhole;

namespace ph = std::placeholders;

/// Helper trait to deduce type in compile-time. Use deduce<T>::show().
template<class T> class deduce;

class overseer_t::channel_watcher_t {
public:
    /// From client to worker and vise-versa.
    enum close_state_t { none = 0x0, tx = 0x1, rx = 0x2, both = tx | rx };

private:
    std::uint64_t channel;
    std::atomic<int> closed;
    std::function<void()> callback;
    std::shared_ptr<overseer_t> overseer;

public:
    channel_watcher_t(std::uint64_t channel, std::shared_ptr<overseer_t> overseer, std::function<void()> callback):
        channel(channel),
        closed(0),
        callback(std::move(callback)),
        overseer(std::move(overseer))
    {}

    /// \pre each closing function must be called exactly once, otherwise the behavior is undefined.
    void
    close(close_state_t state) {
        COCAINE_LOG_TRACE(overseer->log, "closing %s side of %d channel",
                          state == tx ? "tx" : "rx", channel);

        const auto preceding = closed.fetch_or(state);

        BOOST_ASSERT((state & preceding) != state);

        if (closed.load() == close_state_t::both) {
            callback();
        }
    }
};

overseer_t::overseer_t(context_t& context,
           manifest_t manifest, profile_t profile,
           std::shared_ptr<asio::io_service> loop) :
    log(context.log(format("%s/overseer", manifest.name))),
    context(context),
    name(manifest.name),
    loop(loop),
    manifest(manifest),
    profile(profile)
{
    COCAINE_LOG_TRACE(log, "overseer has been initialized");
}

overseer_t::~overseer_t() {
    COCAINE_LOG_TRACE(log, "overseer has been destroyed");
}

locked_ptr<overseer_t::pool_type>
overseer_t::get_pool() {
    return pool.synchronize();
}

locked_ptr<overseer_t::queue_type>
overseer_t::get_queue() {
    return queue.synchronize();
}

void
overseer_t::balance(std::unique_ptr<balancer_t> balancer) {
    if (balancer) {
        this->balancer = std::move(balancer);
    } else {
        this->balancer.reset(new null_balancer_t);
    }
}

std::shared_ptr<streaming_dispatch_t>
overseer_t::enqueue(io::streaming_slot<io::app::enqueue>::upstream_type&& upstream,
                    const std::string& event,
                    const std::string& id)
{
    auto dispatch = std::make_shared<streaming_dispatch_t>(manifest.name);

    auto payload = queue_value { event, id, dispatch, std::move(upstream) };

    if (auto slave = balancer->on_request(event, id)) {
        COCAINE_LOG_TRACE(log, "found a slave with load %d", slave.load);

        assign(slave.id, *slave.slave, payload);
    } else {
        COCAINE_LOG_TRACE(log, "all slaves are busy - delaying the event");

        queue->push(payload);

        balancer->on_queue();
    }

    return dispatch;
}

io::dispatch_ptr_t
overseer_t::handshaker() {
    return std::make_shared<const handshaker_t>(
        name,
        [=](upstream<io::worker::control_tag>&& stream, const std::string& uuid,
            std::shared_ptr<session_t> session) -> std::shared_ptr<control_t>
    {
        scoped_attributes_t holder(*log, {{ "uuid", uuid }});

        COCAINE_LOG_DEBUG(log, "processing handshake message");

        auto control = pool.apply([=](pool_type& pool) -> std::shared_ptr<control_t> {
            auto it = pool.find(uuid);
            if (it == pool.end()) {
                COCAINE_LOG_DEBUG(log, "rejecting slave as unexpected");
                return nullptr;
            }

            COCAINE_LOG_DEBUG(log, "activating slave");
            try {
                return it->second.slave.activate(session, std::move(stream));
            } catch (const std::exception& err) {
                // The slave can be in invalid state; broken, for example, or because the
                // overseer is overloaded. In fact I hope it never happens.
                // Also unlikely we can receive here std::bad_alloc if unable to allocate more
                // memory for control dispatch.
                // If this happens the session will be closed.
                COCAINE_LOG_ERROR(log, "failed to activate the slave: %s", err.what());
            }

            return nullptr;
        });

        if (control) {
            balancer->on_slave_spawn(uuid);
        }

        return control;
    });
}

void
overseer_t::spawn() {
    auto pool = this->pool.synchronize();
    spawn(pool);
}

void
overseer_t::spawn(locked_ptr<pool_type>& pool) {
    COCAINE_LOG_INFO(log, "enlarging the slaves pool to %d", pool->size() + 1);

    slave_context ctx(context, manifest, profile);

    // It is guaranteed that the cleanup handler will not be invoked from within the slave's
    // constructor.
    const auto uuid = ctx.id;
    pool->emplace(uuid, slave_t(std::move(ctx), *loop, [=](const std::error_code& ec){
        if (ec) {
            COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool: %s", ec.message());
        } else {
            COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool");
        }

        this->pool->erase(uuid);
        this->balancer->on_slave_death(uuid);
    }));
}

void
overseer_t::assign(const std::string& id, slave_handler_t& slave, queue_value& payload) {
    const auto channel = balancer->on_channel_started(id);

    auto watcher = std::make_shared<channel_watcher_t>(channel, shared_from_this(), [=]{
        pool.apply([=](pool_type& pool){
            auto it = pool.find(id);
            if (it == pool.end()) {
                return;
            }

            it->second.load--;
            COCAINE_LOG_TRACE(log, "decrease slave load to %d", it->second.load)("uuid", id);
        });

        balancer->on_channel_finished(id, channel);
    });

    auto dispatch = std::make_shared<const worker_client_dispatch_t>(
        payload.upstream,
        std::bind(&channel_watcher_t::close, watcher, channel_watcher_t::rx)
    );

    slave.load++;
    COCAINE_LOG_TRACE(log, "increase slave load to %d", slave.load)("uuid", id);

    auto stream = slave.slave.inject(dispatch);
    stream->send<io::worker::rpc::invoke>(payload.event);

    payload.dispatch->attach(
        std::move(stream),
        std::bind(&channel_watcher_t::close, watcher, channel_watcher_t::tx)
    );
}

void
overseer_t::despawn(std::string, bool /*graceful*/) {}
