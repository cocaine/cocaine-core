/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/detail/locator.hpp"

#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/resolver.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"
#include "cocaine/asio/timeout.hpp"
#include "cocaine/asio/udp.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

#include <random>

using namespace cocaine;
using namespace std::placeholders;

struct locator_t::synchronize_t:
    public slot_concept_t
{
    synchronize_t(locator_t* self):
        slot_concept_t("synchronize"),
        m_packer(m_buffer),
        m_self(self)
    { }

    virtual
    void
    operator()(const msgpack::object&, const api::stream_ptr_t& upstream) {
        dump(upstream);

        // Save this upstream for the future notifications.
        m_upstreams.push_back(upstream);
    }

    void
    update() {
        auto disconnected = std::partition(
            m_upstreams.begin(),
            m_upstreams.end(),
            std::bind(&synchronize_t::dump, this, _1)
        );

        m_upstreams.erase(disconnected, m_upstreams.end());
    }

    void
    shutdown() {
        std::for_each(
            m_upstreams.begin(),
            m_upstreams.end(),
            std::bind(&synchronize_t::close, _1)
        );
    }

private:
    bool
    dump(const api::stream_ptr_t& upstream) {
        m_buffer.clear();

        io::type_traits<synchronize_result_type>::pack(
            m_packer,
            m_self->dump()
        );

        try {
            upstream->write(m_buffer.data(), m_buffer.size());
        } catch(...) {
            return false;
        }

        return true;
    }

    static
    void
    close(const api::stream_ptr_t& upstream) {
        try {
            upstream->close();
        } catch(...) {
            // Ignore.
        }
    }

private:
    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;

    locator_t* m_self;

    std::vector<api::stream_ptr_t> m_upstreams;
};

locator_t::locator_t(context_t& context, io::reactor_t& reactor):
    dispatch_t(context, "service/locator"),
    m_context(context),
    m_log(new logging::log_t(context, "service/locator")),
    m_reactor(reactor)
{
    m_synchronizer = std::make_shared<synchronize_t>(this);

    on<io::locator::resolve>("resolve", std::bind(&locator_t::resolve, this, _1));
    on<io::locator::synchronize>(m_synchronizer);

    if(context.config.network.group.empty()) {
        return;
    }

    auto endpoint = io::udp::endpoint(context.config.network.group, 0);

    if(context.config.network.aggregate) {
        io::udp::endpoint bindpoint("0.0.0.0", 10054);

        m_sink.reset(new io::socket<io::udp>());

        if(::bind(m_sink->fd(), bindpoint.data(), bindpoint.size()) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to bind an announce socket");
        }

        COCAINE_LOG_INFO(m_log, "joining multicast group '%s' on '%s'", endpoint.address(), bindpoint);

        group_req request;

        std::memset(&request, 0, sizeof(request));

        request.gr_interface = 0;

        std::memcpy(&request.gr_group, endpoint.data(), endpoint.size());

        if(::setsockopt(m_sink->fd(), IPPROTO_IP, MCAST_JOIN_GROUP, &request, sizeof(request)) != 0) {
            throw std::system_error(errno, std::system_category(), "unable to join a multicast group");
        }

        m_sink_watcher.reset(new ev::io(m_reactor.native()));
        m_sink_watcher->set<locator_t, &locator_t::on_announce_event>(this);
        m_sink_watcher->start(m_sink->fd(), ev::READ);
    }

    endpoint.port(10054);

    COCAINE_LOG_INFO(m_log, "announcing the node on '%s'", endpoint);

    // NOTE: Connect an UDP socket so that we could send announces via write() instead of sendto().
    m_announce.reset(new io::socket<io::udp>(endpoint));

    const int loop = 0;
    const int life = IP_DEFAULT_MULTICAST_TTL;

    // NOTE: I don't think these calls might fail at all.
    ::setsockopt(m_announce->fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
    ::setsockopt(m_announce->fd(), IPPROTO_IP, IP_MULTICAST_TTL,  &life, sizeof(life));

    m_announce_timer.reset(new ev::timer(m_reactor.native()));
    m_announce_timer->set<locator_t, &locator_t::on_announce_timer>(this);
    m_announce_timer->start(0.0f, 5.0f);
}

locator_t::~locator_t() {
    // Notify all the remote clients about this node shutdown.
    m_synchronizer->shutdown();
    m_synchronizer.reset();

    // Disconnect all the remote nodes.
    m_remotes.clear();

    for(auto it = m_services.rbegin(); it != m_services.rend(); ++it) {
        COCAINE_LOG_INFO(m_log, "stopping service '%s'", it->first);

        // Terminate the service's thread.
        it->second->terminate();
    }

    m_services.clear();
}

void
locator_t::attach(const std::string& name, std::unique_ptr<actor_t>&& service) {
    COCAINE_LOG_INFO(
        m_log,
        "publishing service '%s' on port %s",
        name,
        std::get<1>(service->endpoint())
    );

    // Start the service's thread.
    service->run();

    m_services.emplace_back(
        name,
        std::move(service)
    );
}

namespace {
    struct match {
        template<class T>
        bool
        operator()(const T& service) const {
            return name == service.first;
        }

        const std::string name;
    };

    inline
    resolve_result_type
    query(const std::unique_ptr<actor_t>& actor) {
        return resolve_result_type(
            actor->endpoint(),
            1u,
            actor->dispatch().describe()
        );
    }
}

resolve_result_type
locator_t::resolve(const std::string& name) const {
    auto local = std::find_if(m_services.begin(), m_services.end(), match {
        name
    });

    if(local == m_services.end()) {
        remote_service_map_t::const_iterator begin, end;

        std::tie(begin, end) = m_remote_services.equal_range(name);

        if(begin == end) {
            throw cocaine::error_t("the specified service is not available");
        }

        std::random_device device;

#if defined(__clang__) || defined(HAVE_GCC46)
        std::default_random_engine engine(device());
        std::uniform_int_distribution<int> distribution(0, std::distance(begin, end) - 1);
#else
        std::minstd_rand0 engine(device());
        std::uniform_int<int> distribution(0, std::distance(begin, end) - 1);
#endif

        std::advance(begin, distribution(engine));

        const auto& key = std::get<0>(begin->second);

        COCAINE_LOG_DEBUG(
            m_log,
            "providing service '%s' using remote node '%s' on '%s:%d'",
            name,
            std::get<0>(key),
            std::get<1>(key),
            std::get<2>(key)
        );

        return std::get<1>(begin->second);
    }

    return query(local->second);
}

namespace {
    struct dump_to {
        template<class T>
        void
        operator()(const T& service) {
            target[service.first] = query(service.second);
        }

        synchronize_result_type& target;
    };
}

synchronize_result_type
locator_t::dump() const {
    synchronize_result_type result;

    std::for_each(m_services.begin(), m_services.end(), dump_to {
        result
    });

    return result;
}

void
locator_t::on_announce_event(ev::io&, int) {
    char buffer[1024];
    std::error_code ec;

    ssize_t size = m_sink->read(buffer, sizeof(buffer), ec);

    if(size <= 0) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to receive an announce - [%d] %s", ec.value(), ec.message());
        }

        return;
    }

    msgpack::unpacked unpacked;
    msgpack::unpack(&unpacked, buffer, size);

    remote_t::key_type key;

    try {
        key = unpacked.get().as<remote_t::key_type>();
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(m_log, "unable to decode an announce");
        return;
    }

    if(m_remotes.find(key) == m_remotes.end()) {
        std::string uuid;
        std::string hostname;
        uint16_t port;

        std::tie(uuid, hostname, port) = key;

        COCAINE_LOG_INFO(m_log, "discovered node '%s' on '%s:%d'", uuid, hostname, port);

        std::shared_ptr<io::channel<io::socket<io::tcp>>> channel;

        try {
            channel = std::make_shared<io::channel<io::socket<io::tcp>>>(
                m_reactor,
                std::make_shared<io::socket<io::tcp>>(
                    io::resolver<io::tcp>::query(hostname, port)
                )
            );
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(m_log, "unable to connect to node '%s' - %s", hostname, e.what());
            return;
        }

        auto on_response = std::bind(&locator_t::on_response, this, key, _1);
        auto on_shutdown = std::bind(&locator_t::on_shutdown, this, key, _1);
        auto on_timedout = std::bind(&locator_t::on_timedout, this, key);

        channel->wr->bind(on_shutdown);
        channel->rd->bind(on_response, on_shutdown);

        auto timeout = std::make_shared<io::timeout_t>(m_reactor);

        timeout->bind(on_timedout);

        m_remotes[key] = remote_t {
            channel,
            timeout
        };

        channel->wr->write<io::locator::synchronize>(0UL);
    }

    COCAINE_LOG_DEBUG(m_log, "resetting the heartbeat timeout for node '%s'", std::get<0>(key));

    m_remotes[key].timeout->stop();
    m_remotes[key].timeout->start(60.0f);
}

void
locator_t::on_announce_timer(ev::timer&, int) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    packer << remote_t::key_type(
        m_id.string(),
        m_context.config.network.hostname,
        m_context.config.network.locator
    );

    std::error_code ec;

    if(m_announce->write(buffer.data(), buffer.size(), ec) != static_cast<ssize_t>(buffer.size())) {
        if(ec) {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node - [%d] %s", ec.value(), ec.message());
        } else {
            COCAINE_LOG_ERROR(m_log, "unable to announce the node - unexpected exception");
        }
    }
}

void
locator_t::on_response(const remote_t::key_type& key, const io::message_t& message) {
    switch(message.id()) {
        case io::event_traits<io::rpc::chunk>::id: {
            std::string chunk;
            synchronize_result_type dump;

            message.as<io::rpc::chunk>(chunk);

            msgpack::unpacked unpacked;
            msgpack::unpack(&unpacked, chunk.data(), chunk.size());

            unpacked.get() >> dump;

            COCAINE_LOG_INFO(m_log, "discovered %llu services on node '%s'", dump.size(), std::get<0>(key));

            for(auto it = dump.begin(); it != dump.end(); ++it) {
                m_remote_services.insert(std::make_pair(
                    it->first,
                    std::make_tuple(key, it->second)
                ));
            }

            break;
        }

        case io::event_traits<io::rpc::error>::id:
        case io::event_traits<io::rpc::choke>::id:
            COCAINE_LOG_INFO(m_log, "node '%s' has been shut down", std::get<0>(key));

            purge(key);

            break;
    }
}

void
locator_t::on_shutdown(const remote_t::key_type& key, const std::error_code& ec) {
    COCAINE_LOG_INFO(
        m_log,
        "node '%s' has unexpectedly disconnected - [%d] %s",
        std::get<0>(key),
        ec.value(),
        ec.message()
    );

    purge(key);
}

void
locator_t::on_timedout(const remote_t::key_type& key) {
    COCAINE_LOG_WARNING(m_log, "node '%s' has timed out", std::get<0>(key));
    purge(key);
}

void
locator_t::purge(const remote_t::key_type& key) {
    auto it = m_remote_services.begin(),
         end = m_remote_services.end();

    COCAINE_LOG_DEBUG(
        m_log,
        "purging services for node '%s' on '%s:%d'",
        std::get<0>(key),
        std::get<1>(key),
        std::get<2>(key)
    );

    while(it != end) {
        if(std::get<0>(it->second) == key) {
            m_remote_services.erase(it++);
        } else {
            ++it;
        }
    }

    m_remotes.erase(key);
}
