/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/engine.hpp"

#include "cocaine/context.hpp"
#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/session.hpp"
#include "cocaine/slave.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/traits/json.hpp"
#include "cocaine/traits/unique_id.hpp"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include <boost/bind.hpp>
#include <boost/weak_ptr.hpp>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;
using namespace cocaine::logging;

// Session queue

void
session_queue_t::push(const_reference session) {
    if(session->event.policy.urgent) {
        emplace_front(session);
    } else {
        emplace_back(session);
    }
}

namespace {
    struct downstream_t:
        public api::stream_t
    {
        downstream_t(const boost::shared_ptr<session_t>& session):
            m_session(session),
            m_state(state_t::open)
        { }
       
        virtual 
        ~downstream_t() {
            if(m_state != state_t::closed) {
                close();
            }
        }

        virtual
        void
        push(const char * chunk,
             size_t size)
        {
            switch(m_state) {
                case state_t::open: {
                    const boost::shared_ptr<session_t> ptr = m_session.lock();

                    if(ptr) {
                        ptr->send<rpc::chunk>(std::string(chunk, size));
                    }

                    break;
                }

                case state_t::closed:
                    throw cocaine::error_t("the stream has been closed");
            }
        }

        virtual
        void
        error(error_code code,
              const std::string& message)
        {
            switch(m_state) {
                case state_t::open: {
                    m_state = state_t::closed;

                    const boost::shared_ptr<session_t> ptr = m_session.lock();

                    if(ptr) {
                        ptr->send<rpc::error>(static_cast<int>(code), message);
                        ptr->send<rpc::choke>();
                    }

                    break;
                }

                case state_t::closed:
                    throw cocaine::error_t("the stream has been closed");
            }
        }

        virtual
        void
        close() {
            switch(m_state) {
                case state_t::open: {
                    m_state = state_t::closed;

                    const boost::shared_ptr<session_t> ptr = m_session.lock();

                    if(ptr) {
                        ptr->send<rpc::choke>();
                    }
                    
                    break;
                }

                case state_t::closed:
                    throw cocaine::error_t("the stream has been closed");
            }
        }

    private:
        const boost::weak_ptr<session_t> m_session;

        enum class state_t: int {
            open,
            closed
        };

        state_t m_state;
    };
}

// Engine

engine_t::engine_t(context_t& context,
                   const manifest_t& manifest,
                   const profile_t& profile):
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%1%", manifest.name))),
    m_manifest(manifest),
    m_profile(profile),
    m_state(state_t::stopped),
    m_bus(new io::socket_t(context, ZMQ_ROUTER, m_manifest.name)),
    m_ctl(new io::socket_t(context, ZMQ_PAIR)),
    m_bus_watcher(m_loop),
    m_ctl_watcher(m_loop),
    m_bus_checker(m_loop),
    m_ctl_checker(m_loop),
    m_gc_timer(m_loop),
    m_termination_timer(m_loop),
    m_notification(m_loop),
    m_next_id(0)
{
    m_isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );
    
    std::string bus_endpoint = cocaine::format(
        "ipc://%1%/engines/%2%",
        m_context.config.path.runtime,
        m_manifest.name
    );

    try {
        m_bus->bind(bus_endpoint);
    } catch(const zmq::error_t& e) {
        throw configuration_error_t("unable to bind the engine pool channel - %s", e.what());
    }
   
    std::string ctl_endpoint = cocaine::format(
        "inproc://%s",
        m_manifest.name
    );

    try {
        m_ctl->connect(ctl_endpoint);
    } catch(const zmq::error_t& e) {
        throw configuration_error_t("unable to connect to the engine control channel - %s", e.what());
    }
    
    m_bus_watcher.set<engine_t, &engine_t::on_bus_event>(this);
    m_bus_watcher.start(m_bus->fd(), ev::READ);
    m_bus_checker.set<engine_t, &engine_t::on_bus_check>(this);
    m_bus_checker.start();

    m_ctl_watcher.set<engine_t, &engine_t::on_ctl_event>(this);
    m_ctl_watcher.start(m_ctl->fd(), ev::READ);
    m_ctl_checker.set<engine_t, &engine_t::on_ctl_check>(this);
    m_ctl_checker.start();
    
    m_gc_timer.set<engine_t, &engine_t::on_cleanup>(this);
    m_gc_timer.start(5.0f, 5.0f);

    m_notification.set<engine_t, &engine_t::on_notification>(this);
    m_notification.start();
}

engine_t::~engine_t() {
    BOOST_ASSERT(m_state == state_t::stopped);
}

void
engine_t::run() {
    m_state = state_t::running;
    m_loop.loop();
}

boost::shared_ptr<api::stream_t>
engine_t::enqueue(const api::event_t& event,
                  const boost::shared_ptr<api::stream_t>& upstream)
{
    auto session = boost::make_shared<session_t>(
        m_next_id++,
        event,
        upstream
    );

    boost::unique_lock<session_queue_t> lock(m_queue);

    if(m_state != state_t::running) {
        throw cocaine::error_t("engine is not active");
    }

    if(m_profile.queue_limit > 0 &&
       m_queue.size() >= m_profile.queue_limit)
    {
        throw cocaine::error_t("the queue is full");
    }

    m_queue.push(session);
  
    // NOTE: Release the lock so that the notification could be handled
    // immediately as opposed to instantly blocking on the same acquired lock
    // in the engine thread.
    lock.unlock();

    // Pump the queue! 
    m_notification.send();

    return boost::make_shared<downstream_t>(session);
}

void
engine_t::send(const unique_id_t& uuid,
               const std::string& blob)
{
    boost::unique_lock<boost::mutex> lock(m_bus_mutex);
    m_bus->send_multipart(uuid, blob);
}

void
engine_t::send(const unique_id_t& uuid,
               const std::vector<std::string>& blobs)
{
    COCAINE_LOG_DEBUG(
        m_log,
        "sending a batch of %llu messages to slave %s",
        blobs.size(),
        uuid
    );

    size_t i = 0,
           size = blobs.size();

    boost::unique_lock<boost::mutex> lock(m_bus_mutex);

    m_bus->send(uuid, ZMQ_SNDMORE);

    while(i != size) {
        const auto& blob = blobs[i];

        m_bus->send(
            blob,
            ++i != size ? ZMQ_SNDMORE : 0
        );
    }
}

void
engine_t::on_bus_event(ev::io&, int) {
    bool pending = false;

    {
        boost::unique_lock<boost::mutex> lock(m_bus_mutex);
        pending = m_bus->pending();
    }

    if(pending) {
        process_bus_events();
    }
    
    pump();
    balance();
}

void
engine_t::on_bus_check(ev::prepare&, int) {
    m_loop.feed_fd_event(m_bus->fd(), ev::READ);
}

void
engine_t::on_ctl_event(ev::io&, int) {
    m_ctl_checker.stop();

    if(m_ctl->pending()) {
        m_ctl_checker.start();
        process_ctl_events();    
    }
}

void
engine_t::on_ctl_check(ev::prepare&, int) {
    m_loop.feed_fd_event(m_ctl->fd(), ev::READ);
}

void
engine_t::on_cleanup(ev::timer&, int) {
    typedef std::vector<
        pool_map_t::key_type
    > corpse_list_t;
    
    corpse_list_t corpses;

    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        if(it->second->state() == slave_t::state_t::dead) {
            corpses.emplace_back(it->first);
        }
    }

    if(!corpses.empty()) {
        for(corpse_list_t::iterator it = corpses.begin();
            it != corpses.end();
            ++it)
        {
            m_pool.erase(*it);
        }

        COCAINE_LOG_DEBUG(
            m_log,
            "recycled %llu dead %s",
            corpses.size(),
            corpses.size() == 1 ? "slave" : "slaves"
        );
    }
}

void
engine_t::on_notification(ev::async&, int) {
    pump();
}

void
engine_t::on_termination(ev::timer&, int) {
    boost::unique_lock<session_queue_t> lock(m_queue);
    
    COCAINE_LOG_WARNING(m_log, "forcing the engine termination");
    
    stop();
}

void
engine_t::process_bus_events() {
    // NOTE: Try to read RPC calls in bulk, where the maximum size
    // of the bulk is proportional to the number of spawned slaves.
    unsigned int counter = m_pool.size() * defaults::io_bulk_size;

    // RPC payload.
    unique_id_t slave_id(uninitialized);
    std::string blob;

    // Deserialized message.
    io::message_t message;

    // Originating slave.
    pool_map_t::iterator slave;

    while(counter--) {
        {
            boost::unique_lock<boost::mutex> lock(m_bus_mutex);

            scoped_option<
                options::receive_timeout
            > option(*m_bus, 0);
            
            if(!m_bus->recv_multipart(slave_id, blob)) {
                return;
            }
        }

        slave = m_pool.find(slave_id);

        if(slave == m_pool.end() ||
           slave->second->state() == slave_t::state_t::dead)
        {
            COCAINE_LOG_DEBUG(
                m_log,
                "dropping a message from slave %s — slave is inactive",
                slave_id
            );
            
            continue;
        }

        try {
            message = io::codec::unpack(blob);
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "dropping a message from slave %s — %s",
                slave_id,
                e.what()
            );

            continue;
        }

        COCAINE_LOG_DEBUG(
            m_log,
            "received type %d message from slave %s",
            message.id(),
            slave_id
        );

        switch(message.id()) {
            case event_traits<rpc::heartbeat>::id:
                slave->second->on_ping();
                break;

            case event_traits<rpc::suicide>::id: {
                int code;
                std::string reason;

                message.as<rpc::suicide>(code, reason);

                COCAINE_LOG_DEBUG(
                    m_log,
                    "slave %s is committing suicide: %s",
                    slave_id,
                    reason
                );

                m_pool.erase(slave);

                if(code == rpc::suicide::abnormal) {
                    COCAINE_LOG_ERROR(m_log, "the app seems to be broken - stopping");
                    migrate(state_t::broken);
                    return;
                }

                if(m_state != state_t::running && m_pool.empty()) {
                    // If it was the last slave, shut the engine down.
                    stop();
                    return;
                }

                break;
            }

            case event_traits<rpc::chunk>::id: {
                uint64_t session_id;
                std::string chunk;
                
                message.as<rpc::chunk>(session_id, chunk);

                slave->second->on_chunk(session_id, chunk);

                break;
            }
         
            case event_traits<rpc::error>::id: {
                uint64_t session_id;
                int code;
                std::string reason;

                message.as<rpc::error>(session_id, code, reason);
                
                slave->second->on_error(
                    session_id,
                    static_cast<error_code>(code),
                    reason
                );

                break;
            }

            case event_traits<rpc::choke>::id: {
                uint64_t session_id;

                message.as<rpc::choke>(session_id);

                slave->second->on_choke(session_id);

                break;
            }

            default:
                COCAINE_LOG_WARNING(
                    m_log,
                    "dropping unknown type %d message from slave %s",
                    message.id(),
                    slave_id
                );
        }
    }
}

namespace {
    static
    const char*
    describe[] = {
        "running",
        "broken",
        "stopping",
        "stopped"
    };
}

namespace {
    struct active_t {
        typedef bool result_type;

        template<class T>
        bool
        operator()(const T& slave) {
            size_t load = slave.second->load();

            m_accumulator(load);

            return slave.second->state() == slave_t::state_t::active &&
                   load;
        }

        size_t
        median() const {
            return boost::accumulators::median(m_accumulator);
        }

        size_t
        sum() const {
            return boost::accumulators::sum(m_accumulator);
        }

    private:
        boost::accumulators::accumulator_set<
            size_t,
            boost::accumulators::features<
                boost::accumulators::tag::median,
                boost::accumulators::tag::sum
            >
        > m_accumulator;
    };
}

void
engine_t::process_ctl_events() {
    // RPC payload.
    std::string blob;

    if(!m_ctl->recv(blob)) {
        return;
    }

    // Deserialized message.
    io::message_t message;

    try {
        message = io::codec::unpack(blob);
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_ERROR(
            m_log,
            "dropping a control message — %s",
            e.what()
        );

        return;
    }

    switch(message.id()) {
        case event_traits<control::status>::id: {
            Json::Value info(Json::objectValue);

            active_t active;

            size_t active_pool_size = std::count_if(
                m_pool.begin(),
                m_pool.end(),
                boost::bind(boost::ref(active), _1)
            );

            info["load-median"] = static_cast<Json::LargestUInt>(active.median());
            info["queue-depth"] = static_cast<Json::LargestUInt>(m_queue.size());
            info["sessions"]["pending"] = static_cast<Json::LargestUInt>(active.sum());
            info["slaves"]["total"] = static_cast<Json::LargestUInt>(m_pool.size());
            info["slaves"]["busy"] = static_cast<Json::LargestUInt>(active_pool_size);
            info["state"] = describe[static_cast<int>(m_state)];

            m_ctl->send(info);

            break;
        }

        case event_traits<control::terminate>::id:
            migrate(state_t::stopping);
            break;

        default:
            COCAINE_LOG_ERROR(m_log, "dropping unknown type %d control message", message.id());
    }
}

namespace {
    struct load_t {
        template<class T>
        bool
        operator()(const T& lhs, const T& rhs) const {
            return lhs.second->load() < rhs.second->load();
        }
    };
    
    struct available_t {
        available_t(size_t max_):
            max(max_)
        { }

        template<class T>
        bool
        operator()(const T& slave) const {
            return slave.second->state() == slave_t::state_t::active &&
                   slave.second->load() < max;
        }

        const size_t max;
    };

    template<class It, class Compare, class Predicate>
    inline
    It
    min_element_if(It first,
                   It last,
                   Compare compare,
                   Predicate predicate)
    {
        while(first != last && !predicate(*first)) {
            ++first;
        }

        if(first == last) {
            return first;
        }
     
        It result = first;

        while(++first != last) {
	        if(predicate(*first) && compare(*first, *result)) {
	            result = first;
            }
        }

        return result;
    }
}

void
engine_t::pump() {
    while(!m_queue.empty()) {
        pool_map_t::iterator it = min_element_if(
            m_pool.begin(),
            m_pool.end(),
            load_t(),
            available_t(m_profile.concurrency)
        );

        if(it == m_pool.end()) {
            return;
        }

        session_queue_t::value_type session;

        do {
            boost::unique_lock<session_queue_t> lock(m_queue);

            if(m_queue.empty()) {
                return;
            }

            session = m_queue.front();
            m_queue.pop_front();

            // Process the queue head outside the lock, because it might take
            // some considerable amount of time if, for example, the session has
            // expired and there's some heavy-lifting in the error handler.
            lock.unlock();

            if(session->event.policy.deadline &&
               session->event.policy.deadline <= m_loop.now())
            {
                COCAINE_LOG_DEBUG(
                    m_log,
                    "session %s has expired, dropping",
                    session->id
                );

                session->upstream->error(
                    deadline_error,
                    "the session has expired in the queue"
                );

                session.reset();
            }
        } while(!session);

        // Attach the session to the worker.
        it->second->assign(std::move(session));

        // TODO: Check if it helps.
        m_loop.feed_fd_event(m_bus->fd(), ev::READ);
    }
}

void
engine_t::balance() {
    if(m_pool.size() >= m_profile.pool_limit ||
       m_pool.size() * m_profile.grow_threshold >= m_queue.size())
    {
        return;
    }

    unsigned int target = std::min(
        m_profile.pool_limit,
        std::max(
            1UL,
            m_queue.size() / m_profile.grow_threshold
        )
    );
  
    if(target <= m_pool.size()) {
        return;
    }

    COCAINE_LOG_INFO(
        m_log,
        "enlarging the pool from %d to %d slaves",
        m_pool.size(),
        target
    );

    while(m_pool.size() != target) {
        try {
            boost::shared_ptr<slave_t> slave(
                boost::make_shared<slave_t>(
                    m_context,
                    m_manifest,
                    m_profile,
                    *this
                )
            );

            m_pool.emplace(slave->id(), slave);
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves - %s", e.what());
            break;
        }
    }
}

void
engine_t::migrate(state_t target) {
    boost::unique_lock<session_queue_t> lock(m_queue);

    m_state = target;

    if(!m_queue.empty()) {
        COCAINE_LOG_DEBUG(
            m_log,
            "dropping %llu incomplete %s due to the engine state migration",
            m_queue.size(),
            m_queue.size() == 1 ? "session" : "sessions"
        );

        // Abort all the outstanding sessions.
        while(!m_queue.empty()) {
            m_queue.front()->upstream->error(
                resource_error,
                "engine is shutting down"
            );

            m_queue.pop_front();
        }
    }

    unsigned int pending = 0;

    // NOTE: Send the termination event to the active slaves.
    // If there're no active slaves, the engine can terminate right away,
    // otherwise, the engine should wait for the specified timeout for slaves
    // to finish their sessions and, if they are still active, force the termination.

    for(pool_map_t::iterator it = m_pool.begin();
        it != m_pool.end();
        ++it)
    {
        if(it->second->state() == slave_t::state_t::active) {
            it->second->send(io::codec::pack<rpc::terminate>());
            ++pending;
        }
    }

    if(pending) {
        COCAINE_LOG_INFO(
            m_log,
            "waiting for %d active %s to terminate, timeout: %.02f seconds",
            pending,
            pending == 1 ? "slave" : "slaves",
            m_profile.termination_timeout
        );
        
        m_termination_timer.set<engine_t, &engine_t::on_termination>(this);
        m_termination_timer.start(m_profile.termination_timeout);
    } else {
        stop();
    }    
}

void
engine_t::stop() {
    if(m_termination_timer.is_active()) {
        m_termination_timer.stop();
    }

    // NOTE: This will force the slave pool termination.
    m_pool.clear();

    if(m_state == state_t::stopping) {
        m_state = state_t::stopped;
        m_loop.unloop(ev::ALL);
    }
}
