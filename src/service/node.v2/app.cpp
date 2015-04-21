#include "cocaine/detail/service/node.v2/app.hpp"

#include <queue>

#include "cocaine/api/isolate.hpp"

#include "cocaine/context.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/rpc/actor.hpp"
#include "cocaine/rpc/queue.hpp"

#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node.v2/actor.hpp"
#include "cocaine/detail/service/node.v2/match.hpp"
#include "cocaine/detail/service/node.v2/overseer.hpp"
#include "cocaine/detail/service/node.v2/slave.hpp"
#include "cocaine/detail/service/node.v2/slot.hpp"
#include "cocaine/detail/service/node.v2/splitter.hpp"
#include "cocaine/detail/service/node.v2/dispatch/client.hpp"
#include "cocaine/detail/service/node.v2/dispatch/hostess.hpp"
#include "cocaine/detail/service/node.v2/dispatch/worker.hpp"
#include "cocaine/detail/service/node.v2/slave/control.hpp"
#include "cocaine/detail/service/node.v2/balancing/load.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include <tuple>

#include <blackhole/scoped_attributes.hpp>

using namespace cocaine;
using namespace cocaine::service::v2;

using namespace blackhole;

namespace ph = std::placeholders;

/// Helper trait to deduce type in compile-time. Use deduce<T>::show().
template<class T> class deduce;

struct channel_watcher_t {
    std::function<void()> cb;

    channel_watcher_t(std::function<void()> cb) : cb(cb) {}

    void
    close_tx() {
        BOOST_ASSERT(!tx_closed);
        tx_closed = true;

        check();
    }

    void
    close_rx() {
        BOOST_ASSERT(!rx_closed);
        rx_closed = true;

        check();
    }

private:
    bool rx_closed = false;
    bool tx_closed = false;

    void
    check() {
        if (tx_closed && rx_closed) {
            cb();
        }
    }
};

/// Initial dispatch for slaves.
///
/// Accepts only handshake messages and forwards it to the actual checker (i.e. to the Overseer).
class handshaker_t :
    public dispatch<io::worker_tag>
{
    mutable std::shared_ptr<session_t> session;
    mutable std::mutex mutex;
    mutable std::condition_variable cv;

public:
    template<class F>
    handshaker_t(const std::string& name, F&& fn) :
        dispatch<io::worker_tag>(format("%s/auth", name))
    {
        typedef io::streaming_slot<io::worker::handshake> slot_type;

        on<io::worker::handshake>(std::make_shared<slot_type>(
            [=](slot_type::upstream_type& stream, const std::string& uuid) -> std::shared_ptr<control_t>
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!session) {
                cv.wait(lock);
            }

            return fn(stream, uuid, session);
        }));
    }

    /// Here we need mutable variables, because io::dispatch_ptr_t is a shared pointer over constant
    /// dispatch.
    void bind(std::shared_ptr<session_t> session) const {
        std::unique_lock<std::mutex> lock(mutex);
        this->session = std::move(session);
        lock.unlock();
        cv.notify_one();
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
{}

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
overseer_t::attach(std::shared_ptr<balancer_t> balancer) {
    balancer->attach(shared_from_this());
    this->balancer = std::move(balancer);
}

std::shared_ptr<streaming_dispatch_t>
overseer_t::enqueue(io::streaming_slot<io::app::enqueue>::upstream_type& upstream,
                    const std::string& event,
                    const std::string& id) {
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
        [=](upstream<io::worker::control_tag>& stream, const std::string& uuid,
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

    auto watcher = std::make_shared<channel_watcher_t>([=]{
        pool.apply([=](pool_type& pool){
            auto it = pool.find(id);
            if (it == pool.end()) {
                return;
            }

            it->second.load--;
        });

        balancer->on_channel_finished(id, channel);
    });

    auto dispatch = std::make_shared<const worker_client_dispatch_t>(
        payload.upstream,
        std::bind(&channel_watcher_t::close_rx, watcher)
    );

    slave.load++;
    auto stream = slave.slave.inject(dispatch);
    stream->send<io::worker::rpc::invoke>(payload.event);

    payload.dispatch->attach(
        std::make_shared<
            cocaine::upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>
        >(stream),
        std::bind(&channel_watcher_t::close_tx, watcher)
    );
}

void
overseer_t::despawn(std::string, bool /*graceful*/) {}

/// App dispatch, manages incoming enqueue requests. Adds them to the queue.
class app_dispatch_t:
    public dispatch<io::app_tag>
{
    typedef io::streaming_slot<io::app::enqueue> slot_type;

    std::unique_ptr<logging::log_t> log;

    // Yes, weak pointer here indicates about application destruction.
    std::weak_ptr<overseer_t> overseer;

public:
    app_dispatch_t(context_t& context, const std::string& name, std::shared_ptr<overseer_t> overseer) :
        dispatch<io::app_tag>(name),
        log(context.log(format("app/%s/dispatch", name))),
        overseer(overseer)
    {
        on<io::app::enqueue>(std::make_shared<slot_type>(
            std::bind(&app_dispatch_t::on_enqueue, this, ph::_1, ph::_2, ph::_3)
        ));
    }

    ~app_dispatch_t() {
        COCAINE_LOG_TRACE(log, "app dispatch has been destroyed");
    }

    void
    discard(const std::error_code& ec) const {
        COCAINE_LOG_TRACE(log, "app dispatch has been discarded: %s", ec.message());
    }

private:
    std::shared_ptr<const slot_type::dispatch_type>
    on_enqueue(slot_type::upstream_type& upstream , const std::string& event, const std::string& id) {
        COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event", event);

        if (auto overseer = this->overseer.lock()) {
            return overseer->enqueue(upstream, event, id);
        } else {
            // TODO: Assign error code instead of magic.
            upstream.send<
                io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope::error
            >(42, std::string("the application has been closed"));
            return nullptr;
        }
    }
};

/// Represents a single application. Starts TCP and UNIX servers.
app_t::app_t(context_t& context, const std::string& manifest, const std::string& profile) :
    context(context),
    log(context.log(format("app/%s", manifest))),
    manifest(new manifest_t(context, manifest)),
    profile(new profile_t(context, profile)),
    loop(std::make_shared<asio::io_service>())
{
    auto isolate = context.get<api::isolate_t>(
        this->profile->isolate.type,
        context,
        this->manifest->name,
        this->profile->isolate.args
    );

    // TODO: Start the service immediately, but set its state to `spooling` or somethinh else.
    // While in this state it can serve requests, but always return `invalid state` error.
    if(this->manifest->source() != cached<dynamic_t>::sources::cache) {
        isolate->spool();
    }

    start();
}

app_t::~app_t() {
    COCAINE_LOG_DEBUG(log, "removing application service from the context");
    // TODO: Anounce all opened sessions to be closed (and sockets). But how? For now I've hacked
    // it using weak_ptr.

    balancer->attach(nullptr);

    context.remove(manifest->name)->prototype().discard(std::error_code());

    engine->terminate();
}

void app_t::start() {
    // Load balancer.
    balancer = std::make_shared<load_balancer_t>();

    // Create an overseer - a thing, that watches the event queue and has an ability to spawn or
    // despawn slaves.
    overseer.reset(new overseer_t(context, *manifest, *profile, loop));
    overseer->attach(balancer);

    // Create an TCP server.
    context.insert(manifest->name, std::make_unique<actor_t>(
        context,
        loop,
        std::make_unique<app_dispatch_t>(context, manifest->name, overseer)
    ));

    // Create an unix actor and bind to {manifest->name}.{int} unix-socket.
    engine.reset(new unix_actor_t(
        context,
        manifest->endpoint,
        std::bind(&overseer_t::handshaker, overseer),
        [](io::dispatch_ptr_t handshaker, std::shared_ptr<session_t> session) {
            std::static_pointer_cast<const handshaker_t>(handshaker)->bind(session);
        },
        std::make_shared<asio::io_service>(),
        std::make_unique<hostess_t>(manifest->name)
    ));
    engine->run();
}
