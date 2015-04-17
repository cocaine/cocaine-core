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
#include "cocaine/detail/service/node.v2/dispatch/worker.hpp"

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

/// Initial dispatch for slaves.
///
/// Accepts only handshake messages and forwards it to the actual checker (i.e. to the Overseer).
class authenticator_t :
    public dispatch<io::worker_tag>
{
    mutable std::shared_ptr<session_t> session;
    mutable std::mutex mutex;
    mutable std::condition_variable cv;

public:
    template<class F>
    authenticator_t(const std::string& name, F&& fn) :
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

class cocaine::overseer_t {
public:
    std::unique_ptr<logging::log_t> log;

    context_t& context;

    /// Application name.
    std::string name;

    /// IO loop for timers and standard output fetchers.
    std::shared_ptr<asio::io_service> loop;

    /// Slave pool.
    typedef std::unordered_map<std::string, slave_t> pool_type;
    synchronized<pool_type> pool;

    /// Pending queue.
    struct queue_value {
        std::string event;
        std::string tag;
        std::shared_ptr<streaming_dispatch_t> dispatch;
        io::streaming_slot<io::app::enqueue>::upstream_type upstream;
    };

    synchronized<std::queue<queue_value>> queue;

    std::shared_ptr<balancer_t> balancer;

    manifest_t manifest;
    profile_t profile;

public:
    struct info_t {
        size_t pool;

        info_t(const pool_type& pool) :
            pool(pool.size())
        {}
    };

    overseer_t(context_t& context,
               manifest_t manifest, profile_t profile,
               std::shared_ptr<asio::io_service> loop,
               std::shared_ptr<balancer_t> balancer) :
        log(context.log(format("%s/overseer", manifest.name))),
        context(context),
        name(manifest.name),
        loop(loop),
        balancer(std::move(balancer)),
        manifest(manifest),
        profile(profile)
    {}

    ~overseer_t() {
        COCAINE_LOG_DEBUG(log, "performing overseer shutdown");
    }

    /// If uuid provided - find uuid in pool.
    ///  - found - attach without balancer.
    ///  - not found - ask balancer for create worker with tag and queue (with tag).
    /// If uuid not provided - ask balancer for rebalance.
    /// BALANCER:
    ///  - pool is empty - create adapter, cache event and return. Save adapter in queue.
    ///  - pool is not empty - all unattached - create adapter, cache event and return. Save adapter in queue.
    ///  - pool is not empty - get session (attached) - inject in session. Get upstream.
    ///
    /// \param upstream represents the client <- worker stream.
    /// \param event an invocation event name.
    std::shared_ptr<streaming_dispatch_t>
    enqueue(io::streaming_slot<io::app::enqueue>::upstream_type& upstream, const std::string& event) {
        if (auto dispatch = balancer->queue_changed(upstream, event)) {
            return dispatch;
        }

        auto dispatch = std::make_shared<streaming_dispatch_t>(manifest.name);
        queue->push({ event, "", dispatch, std::move(upstream) });

        return dispatch;
    }

    /// Creates a new authenticate dispatch for incoming connection.
    ///
    /// Called when an unix-socket client (probably, a worker) has been accepted. The first message
    /// from it should be a handshake to be sure, that the remote peer is the worker we are waiting
    /// for.
    /// The handshake message should contain its peer id (likely uuid) by comparing that we either
    /// accept the session or drop it.
    /// After successful accepting the balancer should be notified about pool's changes.
    io::dispatch_ptr_t
    prototype() {
        return std::make_shared<const authenticator_t>(name,
            [=](io::streaming_slot<io::worker::handshake>::upstream_type& /*upstream*/,
                const std::string& uuid, std::shared_ptr<session_t> session)
                -> std::shared_ptr<control_t>
        {
            scoped_attributes_t holder(*log, {{ "uuid", uuid }});

            COCAINE_LOG_DEBUG(log, "processing handshake message");

            auto control = pool.apply([=](pool_type& pool) -> std::shared_ptr<control_t> {
                auto it = pool.find(uuid);
                if (it == pool.end()) {
                    COCAINE_LOG_DEBUG(log, "rejecting slave as unexpected");
                    return nullptr;
                }

                COCAINE_LOG_DEBUG(log, "activating authenticated slave");
                try {
                    auto control = std::make_shared<control_t>(context, name, uuid);
                    it->second.activate(session, control);
                    return control;
                } catch (const std::exception& err) {
                    // The slave can be in invalid state; broken, for example or because Cocaine is
                    // overloaded. In fact I hope it never happens.
                    // Also unlikely we can receive here std::bad_alloc if unable to allocate more
                    // memory for control dispatch.
                    // If this happens the session will be closed.
                    COCAINE_LOG_ERROR(log, "failed to activate the slave: %s", err.what());
                }

                return nullptr;
            });

            if (control) {
                balancer->pool_changed();
            }

            return control;
        });
    }

    /// THESE METHODS ARE IN HEAVY DEVELOPMENT.
    locked_ptr<std::queue<queue_value>> get_queue() { return queue.synchronize(); }
    locked_ptr<pool_type> get_pool() { return pool.synchronize(); }
    locked_ptr<const pool_type> get_pool() const { return pool.synchronize(); }
    info_t info() const { return info_t(*get_pool()); }
    /// ======================================

    /// Spawns a new slave using current manifest and profile.
    ///
    /// Check current pool size. Return if already >=.
    /// Spawn.
    /// Add uuid to map.
    /// Wait for startup timeout. Erase on timeout.
    /// Wait for uuid on acceptor.
    void spawn() {
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

            pool->erase(uuid);
        }));
    }

    /// Closes the worker from new requests
    /// Then calls the overlord to send terminate event. Start timer.
    /// On timeout or on response erases drone.
    void despawn(std::string, bool graceful = true);
};

class simple_balancer_t : public balancer_t {
    std::shared_ptr<overseer_t> overseer;

public:
    void attach(std::shared_ptr<overseer_t> overseer) {
        this->overseer = overseer;
    }

    virtual std::shared_ptr<streaming_dispatch_t>
    queue_changed(io::streaming_slot<io::app::enqueue>::upstream_type& /*wcu*/, std::string /*event*/) {
        auto info = overseer->info();

        // TODO: Insert normal spawn condition here.
        if (info.pool == 0) {
            overseer->spawn();
            return nullptr;
        }

//        auto pool = overseer->get_pool();

//        // TODO: Get slave with minimum load.
//        auto slave = pool->begin()->second;
//        if (auto slave_ = boost::get<std::shared_ptr<slave::active_t>>(&slave)) {
//            auto cwu = (*slave_)->inject(std::make_shared<worker_client_dispatch_t>(wcu));
//            cwu->send<io::worker::rpc::invoke>(event);

//            auto dispatch = std::make_shared<streaming_dispatch_t>("c->w");
//            dispatch->attach(std::make_shared<upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>>(cwu));
//            return dispatch;
//        } else {
//            return nullptr;
//        }
        return nullptr;
    }

    void pool_changed() {
        {
            auto queue = overseer->get_queue();
            if (queue->empty()) {
                return;
            }
        }

//        auto payload = [&]() -> overseer_t::queue_value {
//            auto queue = overseer->get_queue();

//            auto payload = queue->front();
//            queue->pop();
//            return payload;
//        }();

//        auto pool = overseer->get_pool();

//        auto slave = pool->begin()->second;
//        if (auto slave_ = boost::get<std::shared_ptr<slave::active_t>>(&slave)) {
//            auto cwu = (*slave_)->inject(std::make_shared<worker_client_dispatch_t>(payload.upstream));
//            cwu->send<io::worker::rpc::invoke>(payload.event);

//            auto dispatch = payload.dispatch;
//            dispatch->attach(std::make_shared<upstream<io::event_traits<io::worker::rpc::invoke>::dispatch_type>>(cwu));
//        }
    }
};

/// App dispatch, manages incoming enqueue requests. Adds them to the queue.
class app_dispatch_t:
    public dispatch<io::app_tag>
{
    typedef io::streaming_slot<io::app::enqueue> slot_type;

    std::unique_ptr<logging::log_t> log;

    std::shared_ptr<overseer_t> overseer;

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

    ~app_dispatch_t() {}

private:
    std::shared_ptr<const slot_type::dispatch_type>
    on_enqueue(slot_type::upstream_type& upstream , const std::string& event, const std::string& tag) {
        if(tag.empty()) {
            COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event", event);

            return overseer->enqueue(upstream, event);
        } else {
            COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event with tag '%s'", event, tag);
            // TODO: Complete!
            throw cocaine::error_t("on_enqueue: not implemented yet");
        }
    }
};

/// The basic prototype.
///
/// It's here only, because Cocaine API wants it in actor. Does nothing, because it is always
/// replaced by an authenticate dispatch for every incoming connection.
class hostess_t :
    public dispatch<io::worker_tag>
{
public:
    hostess_t(const std::string& name) :
        dispatch<io::worker_tag>(format("%s/hostess", name))
    {}
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
    // TODO: Anounce all opened sessions to be closed (and sockets).

    context.remove(manifest->name);

    engine->terminate();
}

void app_t::start() {
    std::shared_ptr<balancer_t> balancer(new simple_balancer_t);

    // Create an overseer - a thing, that watches the event queue and has an ability to spawn or
    // despawn slaves.
    overseer.reset(new overseer_t(context, *manifest, *profile, loop, balancer));
    balancer->attach(overseer);

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
        std::bind(&overseer_t::prototype, overseer.get()),
        [](io::dispatch_ptr_t auth, std::shared_ptr<session_t> session) {
            std::static_pointer_cast<const authenticator_t>(auth)->bind(session);
        },
        std::make_shared<asio::io_service>(),
        std::make_unique<hostess_t>(manifest->name)
    ));
    engine->run();
}
