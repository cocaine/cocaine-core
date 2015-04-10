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
    mutable std::shared_ptr<session_t> session;
    mutable std::mutex mutex;
    mutable std::condition_variable cv;

public:
    template<class F>
    authenticator_t(const std::string& name, F&& fn) :
        dispatch<io::rpc_tag>(format("%s/auth", name))
    {
        on<io::rpc::handshake>(std::make_shared<io::streaming_slot<io::rpc::handshake>>([=](io::streaming_slot<io::rpc::handshake>::upstream_type& us, const std::string& uuid) -> std::shared_ptr<control_t> {
            std::unique_lock<std::mutex> lock(mutex);
            if (!session) {
                cv.wait(lock);
            }

            return fn(us, uuid, session);
        }));
    }

    void bind(std::shared_ptr<session_t> session) const {
        std::unique_lock<std::mutex> lock(mutex);
        this->session = session;
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

    /// Io loop for timers and standard output fetchers.
    std::shared_ptr<asio::io_service> loop;

    /// Slave pool.
    typedef boost::variant<
        std::shared_ptr<slave::spawning_t>,
        std::shared_ptr<slave::unauthenticated_t>,
        std::shared_ptr<slave::active_t>
        // TODO: closed - closed for new events, but processing current.
        // TODO: terminating - waiting for terminate ack.
    > slave_variant;

    typedef std::unordered_map<std::string, slave_variant> pool_type;

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

    overseer_t(context_t& context, manifest_t manifest, profile_t profile, std::shared_ptr<asio::io_service> loop, std::shared_ptr<balancer_t> balancer) :
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
        balancer->queue_changed(event);

        auto dispatch = std::make_shared<streaming_dispatch_t>(manifest.name);
        queue->push({ event, "", dispatch, std::move(upstream) });

        return dispatch;
    }

    /// Called when an unix-socket client (probably, a worker) has been accepted.
    /// The first message should be a handshake to be sure, that the client is the worker we are
    /// waiting for.
    /// Attach handshake dispatch (with parent=this, session=session).
    /// If handshake's uuid valid - add session to map and notify balancer. Start controlling.
    /// If invalid - drop.
    /// BALANCER:
    ///  - for each pending queue needed for invoke: session->inject(event) -> upstream; adapter->attach(upstream).
    io::dispatch_ptr_t
    prototype() {
        return std::make_shared<const authenticator_t>(name, [=](io::streaming_slot<io::rpc::handshake>::upstream_type&, const std::string& uuid, std::shared_ptr<session_t>) -> std::shared_ptr<control_t> {
            scoped_attributes_t holder(*log, {{ "uuid", uuid }});

            COCAINE_LOG_DEBUG(log, "processing handshake message");

            auto control = pool.apply([=](pool_type& pool) -> std::shared_ptr<control_t> {
                auto it = pool.find(uuid);
                if (it == pool.end()) {
                    COCAINE_LOG_DEBUG(log, "rejecting drone as unexpected");
                    return nullptr;
                }

                COCAINE_LOG_DEBUG(log, "accepted authenticated drone");
                auto control = std::make_shared<control_t>(context, name, uuid);
                it->second = match<slave_variant>(it->second, [](std::shared_ptr<slave::spawning_t>&) -> slave_variant{
                    throw std::runtime_error("invalid state");
                }, [=](std::shared_ptr<slave::unauthenticated_t>& slave) -> slave_variant{
                    return slave->activate(control);
                }, [](std::shared_ptr<slave::active_t>&) -> slave_variant {
                    throw std::runtime_error("invalid state");
                });
                return control;
            });

            if (control) {
                balancer->pool_changed();
            }

            return control;
        });
    }

    locked_ptr<std::queue<queue_value>>
    get_queue() {
        return queue.synchronize();
    }

    info_t
    info() const {
        return info_t(*pool.synchronize());
    }

    /// Spawns a slave using given manifest and profile.
    ///
    /// Check current pool size. Return if already >=.
    /// Spawn.
    /// Add uuid to map.
    /// Wait for startup timeout. Erase on timeout.
    /// Wait for uuid on acceptor.
    void spawn() {
        const size_t size = pool->size();

        COCAINE_LOG_INFO(log, "enlarging the slaves pool from %d to %d", size, size + 1);

        // TODO: Keep logs collector somewhere else.
        auto splitter = std::make_shared<splitter_t>();
        slave_data d(manifest, profile, [=](const std::string& output){
            splitter->consume(output);
            while (auto line = splitter->next()) {
                COCAINE_LOG_DEBUG(log, "output: `%s`", *line);
            }
        });

        const auto uuid = d.id;

        COCAINE_LOG_DEBUG(log, "slave is spawning, timeout: %.02f ms", profile.timeout.spawn)("uuid", uuid);

        const auto now = std::chrono::steady_clock::now();

        // Regardless of whether the asynchronous operation completes immediately or not, the
        // handler will not be invoked from within this function.
        pool->emplace(uuid, slave::spawn(context, std::move(d), loop, [=](result<std::shared_ptr<slave::unauthenticated_t>> result){
            match<void>(result, [=](std::shared_ptr<slave::unauthenticated_t> slave){
                const auto end = std::chrono::steady_clock::now();
                COCAINE_LOG_DEBUG(log, "slave has been spawned in %.3fms",
                    std::chrono::duration<float, std::chrono::milliseconds::period>(end - now).count()
                );

                slave->activate_in([=]{
                    COCAINE_LOG_ERROR(log, "unable to activate slave: timeout");

                    pool->erase(uuid);
                });

                pool.apply([=](pool_type& pool){
                    pool[uuid] = slave;
                });
            }, [=](std::error_code ec){
                COCAINE_LOG_ERROR(log, "unable to spawn more slaves: %s", ec.message());

                pool->erase(uuid);
            });
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

    bool queue_changed(std::string) {
        auto info = overseer->info();
        if (info.pool == 0) {
            overseer->spawn();
            return false;
        }

        return false;
    }

    void pool_changed() {
        // TODO: Rebalance.
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

/// Accepts workers, does nothing.
class hostess_t :
    public dispatch<io::rpc_tag>
{
public:
    hostess_t(const std::string& name) :
        dispatch<io::rpc_tag>(format("%s/hostess", name))
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
    // TODO: Anounce all opened sessions to be closed (and sockets).
    engine->terminate();
    context.remove(manifest->name);
}

void app_t::start() {
    std::shared_ptr<balancer_t> balancer(new simple_balancer_t);

    // Create an overseer - a thing, that watches the event queue and makes decision about what
    // to spawn or despawn.
    overseer.reset(new overseer_t(context, *manifest, *profile, loop, balancer));
    balancer->attach(overseer);

    // Create an TCP server.
    context.insert(manifest->name, std::make_unique<actor_t>(
        context,
        loop,
        std::make_unique<app_dispatch_t>(context, manifest->name, overseer))
    );

    // Create an unix actor and bind to {manifest->name}.{int} unix-socket.
    engine.reset(new unix_actor_t(
        context,
        manifest->endpoint,
        std::bind(&overseer_t::prototype, overseer.get()),
        [](io::dispatch_ptr_t auth, std::shared_ptr<session_t> sess) {
            std::dynamic_pointer_cast<const authenticator_t>(auth)->bind(sess);
        },
        loop,
        std::make_unique<hostess_t>(manifest->name)
    ));
    engine->run();
}
