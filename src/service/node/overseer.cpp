#include "cocaine/detail/service/node/overseer.hpp"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

#include <blackhole/scoped_attributes.hpp>

#include "cocaine/context.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node/balancing/base.hpp"
#include "cocaine/detail/service/node/balancing/null.hpp"
#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshaker.hpp"
#include "cocaine/detail/service/node/dispatch/worker.hpp"
#include "cocaine/detail/service/node/slave/control.hpp"
#include "cocaine/detail/service/node/slot.hpp"

#include <boost/accumulators/statistics/extended_p_square.hpp>

namespace ph = std::placeholders;

using namespace cocaine;

struct collector_t {
    std::size_t active;
    std::size_t cumload;

    explicit
    collector_t(const overseer_t::pool_type& pool):
        active{},
        cumload{}
    {
        for (const auto& it : pool) {
            const auto load = it.second.load();
            if (it.second.active() && load) {
                active++;
                cumload += load;
            }
        }
    }
};

overseer_t::overseer_t(context_t& context,
                       manifest_t manifest,
                       profile_t profile,
                       std::shared_ptr<asio::io_service> loop):
    log(context.log(format("%s/overseer", manifest.name))),
    context(context),
    birthstamp(std::chrono::system_clock::now()),
    manifest_(std::move(manifest)),
    profile_(profile),
    loop(loop),
    stats{}
{
    COCAINE_LOG_TRACE(log, "overseer has been initialized");
}

overseer_t::~overseer_t() {
    COCAINE_LOG_TRACE(log, "overseer has been destroyed");
}

std::shared_ptr<asio::io_service>
overseer_t::io_context() const {
    return loop;
}

profile_t
overseer_t::profile() const {
    return *profile_.synchronize();
}

locked_ptr<overseer_t::pool_type>
overseer_t::get_pool() {
    return pool.synchronize();
}

locked_ptr<overseer_t::queue_type>
overseer_t::get_queue() {
    return queue.synchronize();
}

namespace {

// Helper tagged struct.
struct queue_t {
    unsigned long capacity;

    const synchronized<overseer_t::queue_type>* queue;
};

// Helper tagged struct.
struct pool_t {
    unsigned long capacity;

    const synchronized<overseer_t::pool_type>* pool;
};

class info_visitor_t {
    const io::node::info::flags_t flags;

    dynamic_t::object_t& result;

public:
    info_visitor_t(io::node::info::flags_t flags, dynamic_t::object_t* result):
        flags(flags),
        result(*result)
    {}

    void
    visit(const manifest_t& value) {
        if (flags & io::node::info::expand_manifest) {
            result["manifest"] = value.object();
        }
    }

    void
    visit(const profile_t& value) {
        dynamic_t::object_t info;

        // Useful when you want to edit the profile.
        info["name"] = value.name;

        if (flags & io::node::info::expand_profile) {
            info["data"] = value.object();
        }

        result["profile"] = info;
    }

    // Incoming requests.
    void
    visit(std::uint64_t accepted, std::uint64_t rejected) {
        dynamic_t::object_t info;

        info["accepted"] = accepted;
        info["rejected"] = rejected;

        result["requests"] = info;
    }

    // Pending events queue.
    void
    visit(const queue_t& value) {
        dynamic_t::object_t info;

        info["capacity"] = value.capacity;

        const auto now = std::chrono::high_resolution_clock::now();

        value.queue->apply([&](const overseer_t::queue_type& queue) {
            info["depth"] = queue.size();

            typedef overseer_t::queue_type::value_type value_type;

            if (queue.empty()) {
                info["oldest_event_age"] = 0;
            } else {
                const auto min = *boost::min_element(queue |
                    boost::adaptors::transformed(+[](const value_type& cur) {
                        return cur.event.birthstamp;
                    })
                );

                const auto duration = std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(now - min).count();

                info["oldest_event_age"] = duration;
            }
        });

        result["queue"] = info;
    }

    // Response time quantiles over all events.
    void
    visit(const std::vector<stats_t::quantile_t>& value) {
        dynamic_t::object_t info;

        std::array<char, 16> buf;
        for (const auto& quantile : value) {
            if (std::snprintf(buf.data(), buf.size(), "%.2f%%", quantile.probability)) {
                info[buf.data()] = quantile.value;
            }
        }

        result["timings"] = info;
    }

    void
    visit(const pool_t& value) {
        const auto now = std::chrono::high_resolution_clock::now();

        value.pool->apply([&](const overseer_t::pool_type& pool) {
            collector_t collector(pool);

            // Cumulative load on the app over all the slaves.
            result["load"] = collector.cumload;

            dynamic_t::object_t slaves;
            for (const auto& kv : pool) {
                const auto& name = kv.first;
                const auto& slave = kv.second;

                const auto stats = slave.stats();

                dynamic_t::object_t stat;
                stat["accepted"]   = stats.total;
                stat["load:tx"]    = stats.tx;
                stat["load:rx"]    = stats.rx;
                stat["load:total"] = stats.load;
                stat["state"]      = stats.state;
                stat["uptime"]     = slave.uptime();

                // NOTE: Collects profile info.
                const auto profile = slave.profile();

                dynamic_t::object_t profile_info;
                profile_info["name"] = profile.name;
                if (flags & io::node::info::expand_profile) {
                    profile_info["data"] = profile.object();
                }

                stat["profile"] = profile_info;

                if (stats.age) {
                    const auto duration = std::chrono::duration_cast<
                        std::chrono::milliseconds
                    >(now - *stats.age).count();
                    stat["oldest_channel_age"] = duration;
                } else {
                    stat["oldest_channel_age"] = 0;
                }

                slaves[name] = stat;
            }

            dynamic_t::object_t pinfo;
            pinfo["active"]   = collector.active;
            pinfo["idle"]     = pool.size() - collector.active;
            pinfo["capacity"] = value.capacity;
            pinfo["slaves"]   = slaves;

            result["pool"] = pinfo;
        });
    }
};

} // namespace

dynamic_t::object_t
overseer_t::info(io::node::info::flags_t flags) const {
    dynamic_t::object_t result;

    result["uptime"] = uptime().count();

    info_visitor_t visitor(flags, &result);
    visitor.visit(manifest());
    visitor.visit(profile());
    visitor.visit(stats.accepted.load(), stats.rejected.load());
    visitor.visit({ profile().queue_limit, &queue });
    visitor.visit(stats.quantiles());
    visitor.visit({ profile().pool_limit, &pool });

    return result;
}

std::chrono::seconds
overseer_t::uptime() const {
    const auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::seconds>(now - birthstamp);
}

void
overseer_t::set_balancer(std::shared_ptr<balancer_t> balancer) {
    BOOST_ASSERT(balancer);

    this->balancer = std::move(balancer);
}

std::shared_ptr<client_rpc_dispatch_t>
overseer_t::enqueue(io::streaming_slot<io::app::enqueue>::upstream_type&& downstream,
                    app::event_t event,
                    boost::optional<service::node::slave::id_t> /*id*/)
{
    // TODO: Handle id parameter somehow.

    queue.apply([&](queue_type& queue) {
        const auto limit = profile().queue_limit;

        if (queue.size() >= limit && limit > 0) {
            ++stats.rejected;
            throw std::system_error(error::queue_is_full);
        }
    });

    auto dispatch = std::make_shared<client_rpc_dispatch_t>(manifest().name);

    queue->push_back({
        std::move(event),
        dispatch,
        std::move(downstream),
    });

    ++stats.accepted;
    balancer->on_queue();

    return dispatch;
}

io::dispatch_ptr_t
overseer_t::prototype() {
    return std::make_shared<const handshaker_t>(
        manifest().name,
        std::bind(&overseer_t::on_handshake, shared_from_this(), ph::_1, ph::_2, ph::_3)
    );
}

void
overseer_t::spawn() {
    spawn(pool.synchronize());
}

void
overseer_t::spawn(locked_ptr<pool_type>& pool) {
    COCAINE_LOG_INFO(log, "enlarging the slaves pool to %d", pool->size() + 1);

    slave_context ctx(context, manifest(), profile());

    // It is guaranteed that the cleanup handler will not be invoked from within the slave's
    // constructor.
    const auto uuid = ctx.id;
    pool->insert(std::make_pair(
        uuid,
        slave_t(std::move(ctx), *loop, std::bind(&overseer_t::on_slave_death, shared_from_this(), ph::_1, uuid))
    ));
}

void
overseer_t::spawn(locked_ptr<pool_type>&& pool) {
    spawn(pool);
}

void
overseer_t::assign(slave_t& slave, slave::channel_t& payload) {
    // Attempts to inject the new channel into the slave.
    const auto id = slave.id();
    const auto timestamp = payload.event.birthstamp;

    // TODO: Race possible.
    const auto channel = slave.inject(payload, [=](std::uint64_t channel) {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration<
            double,
            std::chrono::milliseconds::period
        >(now - timestamp).count();

        stats.timings.apply([&](stats_t::quantiles_t& timings) {
            timings(elapsed);
        });

        // TODO: Hack, but at least it saves from the deadlock.
        loop->post(std::bind(&balancer_t::on_channel_finished, balancer, id, channel));
    });

    balancer->on_channel_started(id, channel);
}

void
overseer_t::despawn(const std::string& id, despawn_policy_t policy) {
    pool.apply([&](pool_type& pool) {
        auto it = pool.find(id);
        if (it != pool.end()) {
            switch (policy) {
            case despawn_policy_t::graceful:
                it->second.seal();
                break;
            case despawn_policy_t::force:
                pool.erase(it);
                balancer->on_slave_death(id);
                break;
            default:
                BOOST_ASSERT(false);
            }
        }
    });
}

void
overseer_t::terminate() {
    COCAINE_LOG_DEBUG(log, "overseer is processing terminate request");

    set_balancer(std::make_shared<null_balancer_t>());
    pool->clear();
}

std::shared_ptr<control_t>
overseer_t::on_handshake(const std::string& id,
                         std::shared_ptr<session_t> session,
                         upstream<io::worker::control_tag>&& stream)
{
    blackhole::scoped_attributes_t holder(*log, {{ "uuid", id }});

    COCAINE_LOG_DEBUG(log, "processing handshake message");

    auto control = pool.apply([&](pool_type& pool) -> std::shared_ptr<control_t> {
        auto it = pool.find(id);
        if (it == pool.end()) {
            COCAINE_LOG_DEBUG(log, "rejecting slave as unexpected");
            return nullptr;
        }

        COCAINE_LOG_DEBUG(log, "activating slave");
        try {
            return it->second.activate(std::move(session), std::move(stream));
        } catch (const std::exception& err) {
            // The slave can be in invalid state; broken, for example, or because the overseer is
            // overloaded. In fact I hope it never happens.
            // Also unlikely we can receive here std::bad_alloc if unable to allocate more memory
            // for control dispatch.
            // If this happens the session will be closed.
            COCAINE_LOG_ERROR(log, "failed to activate the slave: %s", err.what());
        }

        return nullptr;
    });

    if (control) {
        balancer->on_slave_spawn(id);
    }

    return control;
}

void
overseer_t::on_slave_death(const std::error_code& ec, std::string uuid) {
    if (ec) {
        COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool: %s", ec.message());
    } else {
        COCAINE_LOG_DEBUG(log, "slave has removed itself from the pool");
    }

    pool.apply([&](pool_type& pool) {
        auto it = pool.find(uuid);
        if (it != pool.end()) {
            it->second.terminate(ec);
            pool.erase(it);
        }
    });
    balancer->on_slave_death(uuid);
}
