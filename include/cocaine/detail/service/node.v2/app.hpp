#pragma once

#include <queue>

#include "cocaine/context.hpp"
#include "cocaine/idl/node.hpp"

#include "cocaine/detail/service/node.v2/balancing/base.hpp"
#include "cocaine/detail/service/node.v2/slot.hpp"
#include "cocaine/detail/service/node.v2/slave.hpp"

namespace cocaine {

struct manifest_t;
struct profile_t;

} // namespace cocaine

namespace cocaine {

struct slave_handler_t {
    slave_t slave;
    std::uint64_t load;

    template<class... Args>
    slave_handler_t(Args&&... args) :
        slave(std::forward<Args>(args)...),
        load(0)
    {}
};

class unix_actor_t;
class overseer_t;
class slave_t;
class control_t;
class streaming_dispatch_t;

}

namespace cocaine {

class overseer_t;

class overseer_t:
    public std::enable_shared_from_this<overseer_t>
{
public:
    std::unique_ptr<logging::log_t> log;

    context_t& context;

    /// Application name.
    std::string name;

    /// IO loop for timers and standard output fetchers.
    std::shared_ptr<asio::io_service> loop;

    /// Slave pool.
    typedef std::unordered_map<std::string, slave_handler_t> pool_type;
    synchronized<pool_type> pool;

    /// Pending queue.
    struct queue_value {
        std::string event;
        std::string tag;
        std::shared_ptr<streaming_dispatch_t> dispatch;
        io::streaming_slot<io::app::enqueue>::upstream_type upstream;
    };

    typedef std::queue<queue_value> queue_type;
    synchronized<queue_type> queue;

    std::shared_ptr<balancer_t> balancer;

    manifest_t manifest;
    profile_t profile;

public:
    overseer_t(context_t& context,
               manifest_t manifest, profile_t profile,
               std::shared_ptr<asio::io_service> loop);
    ~overseer_t();

    locked_ptr<pool_type>
    get_pool();

    locked_ptr<queue_type>
    get_queue();

    void
    balance(std::unique_ptr<balancer_t> balancer = nullptr);

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
    enqueue(io::streaming_slot<io::app::enqueue>::upstream_type& upstream,
            const std::string& event,
            const std::string& id);

    /// Creates a new authenticate dispatch for incoming connection.
    ///
    /// Called when an unix-socket client (probably, a worker) has been accepted. The first message
    /// from it should be a handshake to be sure, that the remote peer is the worker we are waiting
    /// for.
    /// The handshake message should contain its peer id (likely uuid) by comparing that we either
    /// accept the session or drop it.
    /// After successful accepting the balancer should be notified about pool's changes.
    io::dispatch_ptr_t
    handshaker();

    /// Spawns a new slave using current manifest and profile.
    ///
    /// Check current pool size. Return if already >=.
    /// Spawn.
    /// Add uuid to map.
    /// Wait for startup timeout. Erase on timeout.
    /// Wait for uuid on acceptor.
    void
    spawn();

    void
    spawn(locked_ptr<pool_type>& pool);

    /// \warning must be called under pool & queue lock.
    void
    assign(const std::string& id, slave_handler_t& slave, queue_value& payload);

    /// Closes the worker from new requests
    /// Then calls the overlord to send terminate event. Start timer.
    /// On timeout or on response erases drone.
    void despawn(std::string, bool graceful = true);
};

}

namespace cocaine { namespace service { namespace v2 {

class app_t {
    context_t& context;

    const std::unique_ptr<logging::log_t> log;

    // Configuration.
    std::unique_ptr<const manifest_t> manifest;
    std::unique_ptr<const profile_t>  profile;

    std::shared_ptr<asio::io_service> loop;
    std::unique_ptr<unix_actor_t> engine;
    std::shared_ptr<overseer_t> overseer;

public:
    app_t(context_t& context, const std::string& manifest, const std::string& profile);
    app_t(const app_t& other) = delete;

    ~app_t();

    app_t& operator=(const app_t& other) = delete;

private:
    void start();
};

}}} // namespace cocaine::service::v2
