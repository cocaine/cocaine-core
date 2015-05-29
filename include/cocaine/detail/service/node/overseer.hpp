#pragma once

#include <string>
#include <queue>

#include "cocaine/logging.hpp"

#include "cocaine/idl/rpc.hpp"
#include "cocaine/idl/node.hpp"

#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/detail/service/node/app/stats.hpp"
#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/slot.hpp"
#include "cocaine/detail/service/node/stream.hpp"

namespace cocaine {

class balancer_t;
class unix_actor_t;
class slave_t;
class control_t;
class client_rpc_dispatch_t;

} // namespace cocaine

namespace cocaine {

class overseer_t:
    public std::enable_shared_from_this<overseer_t>
{
public:
    enum class despawn_policy_t {
        graceful,
        force
    };

    typedef std::unordered_map<
        std::string,
        slave_t
    > pool_type;

    typedef std::queue<
        slave::channel_t
    > queue_type;

private:
    const std::unique_ptr<logging::log_t> log;

    context_t& context;

    /// Time point, when the application was created.
    const std::chrono::high_resolution_clock::time_point birthstamp;

    /// The application manifest.
    const manifest_t manifest;

    /// The application profile.
    synchronized<profile_t> profile_;

    /// IO loop for timers and standard output fetchers.
    std::shared_ptr<asio::io_service> loop;

    /// Slave pool.
    synchronized<pool_type> pool;

    /// Pending queue.
    synchronized<queue_type> queue;

    /// The application balancing policy.
    std::shared_ptr<balancer_t> balancer;

    /// Statistics.
    stats_t stats;

public:
    overseer_t(context_t& context,
               manifest_t manifest,
               profile_t profile,
               std::shared_ptr<asio::io_service> loop);
    ~overseer_t();

    /// \todo not sure about this method necessity, consider passing a shared logger pointer to the
    /// balancer.
    const logging::log_t&
    logger() const {
        return *log;
    }

    /// Returns copy of the current profile, which is used to spawn new slaves.
    ///
    /// \note the current profile may change in any moment. Moveover some slaves can be in some kind
    /// of transition state, i.e. migrating from one profile to another.
    profile_t
    profile() const;

    locked_ptr<pool_type>
    get_pool();

    locked_ptr<queue_type>
    get_queue();

    /// Returns the complete info about how the application works.
    dynamic_t::object_t
    info() const;

    void
    balance(std::unique_ptr<balancer_t> balancer = nullptr);

    /// Enqueues the new event into the most appropriate slave.
    ///
    /// The event will be put into the queue if there are no slaves available at this moment or all
    /// of them are busy.
    ///
    /// \return the dispatch object, which is ready for processing the appropriate protocol
    /// messages.
    ///
    /// \param downstream represents the [Client <- Worker] stream.
    /// \param event an invocation event.
    /// \param id represents slave id to be enqueued (may be none, which means any slave).
    ///
    /// \todo consult with E. guys about deadline policy.
    std::shared_ptr<client_rpc_dispatch_t>
    enqueue(io::streaming_slot<io::app::enqueue>::upstream_type&& downstream,
            app::event_t event,
            boost::optional<slave::id_t> id);

    std::shared_ptr<stream_t>
    enqueue(std::shared_ptr<stream_t>&& downstream, app::event_t event, boost::optional<slave::id_t> id);

    /// Creates a new handshake dispatch for incoming connection.
    ///
    /// Called when an unix-socket client (probably, a worker) has been accepted. The first message
    /// from it should be a handshake to be sure, that the remote peer is the worker we are waiting
    /// for.
    /// The handshake message should contain its peer id (likely uuid) by comparing that we either
    /// accept the session or drop it.
    /// After successful accepting the balancer should be notified about pool's changes.
    io::dispatch_ptr_t
    prototype();

    /// Spawns a new slave using current manifest and profile.
    void
    spawn();

    /// \overload
    void
    spawn(locked_ptr<pool_type>& pool);

    /// \overload
    void
    spawn(locked_ptr<pool_type>&& pool);

    /// \warning must be called under the pool lock.
    void
    assign(slave_t& slave, slave::channel_t& payload);

    /// Closes the worker from new requests
    /// Then forces the slave to send terminate event. Start timer.
    /// On timeout or on response erases slave.
    void
    despawn(const std::string& id, despawn_policy_t policy);

private:
    std::shared_ptr<control_t>
    on_handshake(const std::string& id,
                 std::shared_ptr<session_t> session,
                 upstream<io::worker::control_tag>&& stream);

    void
    on_slave_death(const std::error_code& ec, std::string uuid);
};

}
