#include "cocaine/detail/service/node.v2/app.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/context.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/rpc/actor.hpp"
#include "cocaine/rpc/queue.hpp"

#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node.v2/actor.hpp"
#include "cocaine/detail/service/node.v2/drone.hpp"
#include "cocaine/detail/service/node.v2/slot.hpp"
#include "cocaine/detail/service/node.v2/splitter.hpp"

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

// From client to worker.
class streaming_dispatch_t:
    public dispatch<io::event_traits<io::app::enqueue>::dispatch_type>
{
    typedef io::event_traits<io::app::enqueue>::dispatch_type tag_type;
    typedef io::protocol<tag_type>::scope protocol;

    cocaine::synchronized<io::message_queue<tag_type, upstream<tag_type>>> mq;

public:
    explicit streaming_dispatch_t(const std::string& name):
        dispatch<io::event_traits<io::app::enqueue>::dispatch_type>(name)
    {
        on<protocol::chunk>(std::bind(&streaming_dispatch_t::write, this, ph::_1));
        on<protocol::error>(std::bind(&streaming_dispatch_t::error, this, ph::_1, ph::_2));
        on<protocol::choke>(std::bind(&streaming_dispatch_t::close, this));
    }

    // TODO: Complete.
    void attach(/*upstream*/) {
    }

private:
    void
    write(const std::string& chunk) {
        mq->append<protocol::chunk>(chunk);
    }

    void
    error(int id, const std::string& reason) {
        mq->append<protocol::error>(id, reason);
    }

    void
    close() {
        mq->append<protocol::choke>();
    }
};

/// Initial dispatch for slaves. Accepts only handshake messages and forwards it to the actual
/// checker (i.e. to the Overseer).
class authenticator_t :
    public dispatch<io::rpc_tag>
{
public:
    template<class F>
    authenticator_t(const std::string& name, F&& fn) :
        dispatch<io::rpc_tag>(format("%s/auth", name))
    {
        on<io::rpc::handshake>(std::make_shared<io::streaming_slot<io::rpc::handshake>>(std::move(fn)));
    }
};

/// Control channel for single drone.
/// (Worker should shut itself down after sending terminate message back (even if it initiates) to Runtime).
class control_t :
    public dispatch<io::control_tag>,
    public std::enable_shared_from_this<control_t>
{
    std::unique_ptr<logging::log_t> log;

    /// - timer - heartbeat timer.
    /// - session - session pointer for sending events, like heartbeat and terminate. May detach when it disappears.
public:
    control_t(context_t& context, const std::string& name, const std::string& uuid) :
        dispatch<io::control_tag>(format("%s/control", name)),
        log(context.log(format("%s/control", name), attribute::set_t({{ "uuid", uuid }})))
    {
        on<io::control::heartbeat>([&](){
            COCAINE_LOG_DEBUG(log, "processing heartbeat message");
            // TODO: Send heartbeat back.
            // TODO: Reset heartbeat timer.
        });

        // TODO: `on<io::control::terminate>();` - call detach from parent, send SIGTERM to worker,
        // wait for GRACEFUL_SHUTDOWN_TIMEOUT and send SIGKILL.
    }

    ~control_t() {
    }

    virtual
    void
    discard(const std::error_code&) const override {
        // Unix socket is destroyed some unexpected way, for example worker is down.
        // Detach slave from pool - `overseer->detach(uuid);`.
    }

    void
    reset_idle_timer() {}
};

class cocaine::balancer_t {
public:
    virtual
    void rebalance() = 0;
};

class cocaine::overseer_t {
public:
    context_t& context;

    /// Application name.
    std::string name;

    /// Io loop for timers and standard output fetchers.
    std::shared_ptr<asio::io_service> loop;

    std::unique_ptr<logging::log_t> log;

    /// Slave pool.
    struct slave_context_t {
        std::shared_ptr<slave_t> slave;
        std::shared_ptr<control_t> control;

        slave_context_t(std::shared_ptr<slave_t> slave) : slave(slave) {}
    };

    synchronized<std::unordered_map<std::string, slave_context_t>> pool;

    /// - queue<<event, tag, adapter>> - pending queue.
    /// - balancer - balancing policy.

    /// If uuid provided - find uuid in pool.
    ///  - found - attach without balancer.
    ///  - not found - ask balancer for create worker with tag and queue (with tag).
    /// If uuid not provided - ask balancer for rebalance.
    /// BALANCER:
    ///  - pool is empty - create adapter, cache event and return. Save adapter in queue.
    ///  - pool is not empty - all unattached - create adapter, cache event and return. Save adapter in queue.
    ///  - pool is not empty - get session (attached) - inject in session. Get upstream.

public:
    overseer_t(context_t& context, const std::string& name, std::shared_ptr<asio::io_service> loop) :
        context(context),
        name(name),
        loop(loop),
        log(context.log(format("%s/overseer", name)))
    {}

    std::shared_ptr<streaming_dispatch_t>
    enqueue(io::streaming_slot<io::app::enqueue>::upstream_type& /*upstream*/, const std::string& /*event*/) {
        return nullptr;
    }

    /// Called when an unix-socket client (probably, a worker) has been accepted.
    /// The first message should be a handshake to be sure, that the client is the worker we are
    /// waiting for.
    /// Attach handshake dispatch (with parent=this, session=session).
    /// If handshake's uuid valid - add session to map and notify balancer. Start controlling.
    /// If invalid - drop.
    /// BALANCER:
    ///  - for each pending queue needed for invoke: session->inject(event) -> upstream; adapter->attach(upstream).
    void
    attach(std::shared_ptr<session_t> session) {
        COCAINE_LOG_DEBUG(log, "attaching drone candidate session");

        session->inject(std::make_shared<authenticator_t>(name, [=](io::streaming_slot<io::rpc::handshake>::upstream_type&, const std::string& uuid) -> std::shared_ptr<control_t> {
            scoped_attributes_t holder(*log, {{ "uuid", uuid }});

            COCAINE_LOG_DEBUG(log, "processing handshake message");

            auto control = pool.apply([=](std::unordered_map<std::string, slave_context_t>& pool) -> std::shared_ptr<control_t> {
                auto it = pool.find(uuid);
                if (it == pool.end()) {
                    COCAINE_LOG_DEBUG(log, "rejecting drone as unexpected");
                    return nullptr;
                }

                COCAINE_LOG_DEBUG(log, "accepted authenticated drone");
                auto control = std::make_shared<control_t>(context, name, uuid);
                it->second.control = control;
                return control;
            });

            if (control) {
                // TODO: balancer->pool_changed();
            }

            return control;
        }));
    }

    /// Spawns a slave using given manifest and profile.
    ///
    /// Check current pool size. Return if already >=.
    /// Spawn.
    /// Add uuid to map.
    /// Wait for startup timeout. Erase on timeout.
    /// Wait for uuid on acceptor.
    void spawn(manifest_t manifest, profile_t profile) {
        const size_t size = pool->size();

        COCAINE_LOG_INFO(log, "enlarging the slaves pool from %d to %d", size, size + 1);

        // TODO: Currently it just spawns one more slave synchronously.
        auto splitter = std::make_shared<splitter_t>();
        slave_data d(manifest, profile, [=](const std::string& output){
            splitter->consume(output);
            while (auto line = splitter->next()) {
                COCAINE_LOG_DEBUG(log, "output: `%s`", *line);
            }
        });

        auto uuid = d.id;
        pool->insert(std::make_pair(uuid, slave_context_t(
            slave_t::make(context, std::move(d), loop)
        )));
    }

    /// Closes the worker from new requests
    /// Then calls the overlord to send terminate event. Start timer.
    /// On timeout or on response erases drone.
    void despawn(std::string, bool graceful = true);
};

/// App dispatch, manages incoming enqueue requests. Adds them to the queue.
class app_dispatch_t:
    public dispatch<io::app_tag>
{
    typedef io::streaming_slot<io::app::enqueue> slot_type;

    std::unique_ptr<logging::log_t> log;

    std::shared_ptr<overseer_t> overseer;

public:
    app_dispatch_t(context_t& context, const manifest_t& manifest, std::shared_ptr<overseer_t> overseer) :
        dispatch<io::app_tag>(manifest.name),
        log(context.log(cocaine::format("app/dispatch/%s", manifest.name))),
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

/// Accepts workers, does nothing.
class hostess_t :
    public dispatch<io::rpc_tag>
{
public:
    hostess_t(const std::string& name) :
        dispatch<io::rpc_tag>(format("%s/hostess", name))
    {}
};

//class simple_balancer_t : public balancer_t {
//public:
//    void rebalance() override {
//        if (pool.empty()) {
//            overseer.spawn();
//        }
//    }
//};

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
    // TODO: Anounce all opened sessions to be closed (and sockets).
    engine->terminate();
    context.remove(manifest->name);
}

void app_t::start() {
    // Create an overseer - a thing, that watches the event queue and makes decision about what
    // to spawn or despawn.
    overseer.reset(new overseer_t(context, manifest->name, loop));

    // Create an TCP server.
    context.insert(manifest->name, std::make_unique<actor_t>(
        context,
        loop,
        std::make_unique<app_dispatch_t>(context, *manifest, overseer))
    );

    // Create an unix actor and bind to {manifest->name}.{int} unix-socket.
    engine.reset(new unix_actor_t(
        context,
        manifest->endpoint,
        std::bind(&overseer_t::attach, overseer.get(), ph::_1),
        loop,
        std::make_unique<hostess_t>(manifest->name)
    ));
    engine->run();

    // TODO: Temporary.
    overseer->spawn(*manifest, *profile);
}
