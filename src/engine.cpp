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

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>

#include <boost/bind.hpp>
#include <boost/format.hpp>

#include "cocaine/engine.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/session.hpp"
#include "cocaine/slave.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/traits/json.hpp"
#include "cocaine/traits/unique_id.hpp"

using namespace cocaine;
using namespace cocaine::engine;

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
        downstream_t(const unique_id_t& id):
            m_id(id),
            m_state(states::caching)
        { }
        
        ~downstream_t() {
            if(m_state != states::closed) {
                close();
            }
        }

        virtual
        void
        push(const void * chunk,
             size_t size)
        {
            switch(m_state) {
                case states::caching: {
                    boost::unique_lock<boost::mutex> lock(m_mutex);

                    // NOTE: Put the new chunk into the cache because the stream is
                    // not yet attached to a slave.
                    m_cache.emplace_back(static_cast<const char*>(chunk), size);

                    break;
                }

                case states::open:
                    push_impl(chunk, size);
                    break;

                case states::closed:
                    throw cocaine::error_t("the stream has been closed");
            }
        }

        virtual
        void
        error(error_code,
              const std::string&)
        {
            throw cocaine::error_t("not implemented");
        }

        virtual
        void
        close() {
            switch(m_state) {
                case states::open:
                    // TEST: An attached stream should always have a controlling slave.
                    BOOST_ASSERT(m_slave);

                    m_slave->send(io::message<rpc::choke>(m_id));

                    break;

                case states::closed:
                    throw cocaine::error_t("the stream has been closed");
            }

            m_state = states::closed;
        }

        void
        attach(const boost::shared_ptr<slave_t>& slave) {
            // TEST: Ensure that the stream is in a proper state.
            BOOST_ASSERT(m_state != states::open && !m_slave);

            m_slave = slave;

            boost::unique_lock<boost::mutex> lock(m_mutex);

            if(!m_cache.empty()) {
                for(chunk_list_t::const_iterator it = m_cache.begin();
                    it != m_cache.end();
                    ++it)
                {
                    push_impl(it->data(), it->size());
                }
                
                m_cache.clear();
            }

            if(m_state == states::closed) {
                m_slave->send(io::message<rpc::choke>(m_id));
                return;
            }

            m_state = states::open;
        }

    private:
        void
        push_impl(const void * chunk,
                  size_t size)
        {
            // TEST: An attached stream should always have a controlling slave.
            BOOST_ASSERT(m_slave);

            zmq::message_t message(size);

            memcpy(
                message.data(),
                chunk,
                size
            );

            m_slave->send(io::message<rpc::chunk>(m_id, message));
        }

    private:
        const unique_id_t m_id;

        struct states {
            enum value: int {
                caching,
                open,
                closed
            };
        };

        // Stream state.
        std::atomic<int> m_state;

        typedef std::vector<
            std::string
        > chunk_list_t;

        // Request chunk cache.
        chunk_list_t m_cache;
        boost::mutex m_mutex;

        // Responsible slave.
        boost::shared_ptr<slave_t> m_slave;
    };
}

// Engine

engine_t::engine_t(context_t& context,
                   const manifest_t& manifest,
                   const profile_t& profile):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % manifest.name
        ).str()
    )),
    m_manifest(manifest),
    m_profile(profile),
    m_state(states::stopped),
    m_bus(new rpc_channel_t(context, ZMQ_ROUTER, m_manifest.name)),
    m_ctl(new control_channel_t(context, ZMQ_PAIR)),
    m_bus_watcher(m_loop),
    m_ctl_watcher(m_loop),
    m_bus_checker(m_loop),
    m_ctl_checker(m_loop),
    m_gc_timer(m_loop),
    m_termination_timer(m_loop),
    m_notification(m_loop)
{
    m_isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );
    
    std::string bus_endpoint(
        (boost::format("ipc://%1%/%2%")
            % m_context.config.ipc_path
            % m_manifest.name
        ).str()
    );

    try {
        m_bus->bind(bus_endpoint);
    } catch(const zmq::error_t& e) {
        throw configuration_error_t("unable to bind the engine pool channel - %s", e.what());
    }
   
    std::string ctl_endpoint(
        (boost::format("inproc://%s")
            % m_manifest.name
        ).str()
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
    BOOST_ASSERT(m_state == states::stopped);
}

void
engine_t::run() {
    m_state = states::running;
    m_loop.loop();
}

boost::shared_ptr<api::stream_t>
engine_t::enqueue(const api::event_t& event,
                  const boost::shared_ptr<api::stream_t>& upstream,
                  engine::mode mode)
{
    boost::unique_lock<session_queue_t> lock(m_queue);

    if(m_state != states::running) {
        throw cocaine::error_t("engine is not active");
    }

    if(m_profile.queue_limit > 0) {
        if(mode == engine::mode::normal &&
           m_queue.size() >= m_profile.queue_limit)
        {
            throw cocaine::error_t("the queue is full");
        }

        while(m_queue.size() >= m_profile.queue_limit) {
            m_condition.wait(lock);
        }
    }

    unique_id_t id;

    boost::shared_ptr<session_t> session(
        boost::make_shared<session_t>(
            id,
            event,
            upstream,
            boost::make_shared<downstream_t>(id)
        )
    );

    m_queue.push(session);
  
    // Pump the queue! 
    m_notification.send();

    return session->downstream;
}

void
engine_t::on_bus_event(ev::io&, int) {
    bool pending = false;

    {
        boost::unique_lock<rpc_channel_t> lock(*m_bus);
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
        if(it->second->state() == slave_t::states::dead) {
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
engine_t::on_termination(ev::timer&, int) {
    boost::unique_lock<session_queue_t> lock(m_queue);
    
    COCAINE_LOG_WARNING(m_log, "forcing the engine termination");
    
    stop();
}

void
engine_t::on_notification(ev::async&, int) {
    pump();
}

void
engine_t::process_bus_events() {
    // NOTE: Try to read RPC calls in bulk, where the maximum size
    // of the bulk is proportional to the number of spawned slaves.
    int counter = m_pool.size() * defaults::io_bulk_size;
    
    do {
        boost::unique_lock<rpc_channel_t> lock(*m_bus);

        // TEST: Ensure that we haven't missed something in a previous iteration.
        BOOST_ASSERT(!m_bus->more());
    
        unique_id_t slave_id(uninitialized);
        int command = -1;
        
        {
            io::scoped_option<
                io::options::receive_timeout,
                io::policies::shared
            > option(*m_bus, 0);
            
            if(!m_bus->recv_tuple(boost::tie(slave_id, command))) {
                return;
            }
        }

        pool_map_t::iterator slave(m_pool.find(slave_id));

        if(slave == m_pool.end()) {
            COCAINE_LOG_DEBUG(
                m_log,
                "dropping type %d message from an unknown slave %s", 
                command,
                slave_id
            );
            
            m_bus->drop();
            
            continue;
        }

        COCAINE_LOG_DEBUG(
            m_log,
            "received type %d message from slave %s",
            command,
            slave_id
        );

        switch(command) {
            case io::message<rpc::ping>::value:
                lock.unlock();

                slave->second->process(io::message<rpc::ping>());

                break;

            case io::message<rpc::suicide>::value: {
                int code = 0;
                std::string message;

                m_bus->recv_tuple(boost::tie(code, message));
                lock.unlock();

                m_pool.erase(slave);

                if(code == rpc::suicide::abnormal) {
                    COCAINE_LOG_ERROR(m_log, "the app seems to be broken — %s", message);
                    shutdown(states::broken);
                    return;
                }

                if(m_state != states::running && m_pool.empty()) {
                    // If it was the last slave, shut the engine down.
                    stop();
                    return;
                }

                break;
            }

            case io::message<rpc::chunk>::value: {
                unique_id_t session_id(uninitialized);
                zmq::message_t message;
                
                m_bus->recv_tuple(boost::tie(session_id, message));
                lock.unlock();

                slave->second->process(
                    io::message<rpc::chunk>(session_id, message)
                );

                break;
            }
         
            case io::message<rpc::error>::value: {
                unique_id_t session_id(uninitialized);
                int code = 0;
                std::string message;

                m_bus->recv_tuple(boost::tie(session_id, code, message));
                lock.unlock();

                slave->second->process(
                    io::message<rpc::error>(session_id, code, message)
                );

                break;
            }

            case io::message<rpc::choke>::value: {
                unique_id_t session_id(uninitialized);

                m_bus->recv(session_id);
                lock.unlock();

                slave->second->process(
                    io::message<rpc::choke>(session_id)
                );

                break;
            }

            default:
                COCAINE_LOG_WARNING(
                    m_log,
                    "dropping unknown type %d message from slave %s",
                    command,
                    slave_id
                );

                m_bus->drop();
        }
    } while(--counter);
}

namespace {
    static const char * describe[] = {
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

            return slave.second->state() == slave_t::states::active && load;
        }

        size_t
        median() const {
            return boost::accumulators::median(m_accumulator);
        }

    private:
        boost::accumulators::accumulator_set<
            size_t,
            boost::accumulators::features<
                boost::accumulators::tag::median
            >
        > m_accumulator;
    };
}

void
engine_t::process_ctl_events() {
    int command = -1;

    if(!m_ctl->recv(command)) {
        COCAINE_LOG_ERROR(m_log, "received a corrupted control message");
        m_ctl->drop();
        return;
    }

    switch(command) {
        case io::message<control::status>::value: {
            Json::Value info(Json::objectValue);

            active_t active;

            size_t active_pool_size = std::count_if(
                m_pool.begin(),
                m_pool.end(),
                boost::bind(boost::ref(active), _1)
            );

            info["queue-depth"] = static_cast<Json::LargestUInt>(m_queue.size());
            info["load-median"] = static_cast<Json::LargestUInt>(active.median());
            info["slaves"]["total"] = static_cast<Json::LargestUInt>(m_pool.size());
            info["slaves"]["busy"] = static_cast<Json::LargestUInt>(active_pool_size);
            info["state"] = describe[m_state];

            m_ctl->send(info);

            break;
        }

        case io::message<control::terminate>::value:
            shutdown(states::stopping);
            break;

        default:
            COCAINE_LOG_ERROR(m_log, "received an unknown control message, code: %d", command);
            m_ctl->drop();
    }
}

namespace {
    struct load_t {
        template<class T>
        bool
        operator()(const T& lhs, const T& rhs) {
            return lhs.second->load() < rhs.second->load();
        }
    };
    
    struct available_t {
        available_t(size_t max_):
            max(max_)
        { }

        template<class T>
        bool
        operator()(const T& slave) {
            return slave.second->state() == slave_t::states::active &&
                   slave.second->load() < max;
        }

        const size_t max;
    };

    template<class It, class Compare, class Predicate>
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

        while(!session) {
            boost::unique_lock<session_queue_t> lock(m_queue);

            if(m_queue.empty()) {
                return;
            }

            session = m_queue.front();
            m_queue.pop_front();

            if(session->event.policy.deadline &&
               session->event.policy.deadline <= m_loop.now())
            {
                session->abandon(
                    deadline_error,
                    "the session has expired in the queue"
                );

                session.reset();
            }
        }

        // Notify one of the blocked enqueue operations.
        m_condition.notify_one();
       
        io::message<rpc::invoke> message(
            session->id,
            session->event.type
        );

        send(it->first, message);        

        it->second->assign(session);

        static_cast<downstream_t&>(
            *session->downstream
        ).attach(it->second);

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

    // NOTE: Balance the slave pool in order to keep it in a proper shape
    // based on the queue size and other policies.
    
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
                    this
                )
            );

            m_pool.emplace(slave->id(), slave);
        } catch(const system_error_t& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to spawn more slaves - %s - %s",
                e.what(),
                e.reason()
            );

            return;
        }
    }
}

void
engine_t::shutdown(states::value target) {
    boost::unique_lock<session_queue_t> lock(m_queue);

    m_state = target;

    if(!m_queue.empty()) {
        COCAINE_LOG_DEBUG(
            m_log,
            "dropping %llu incomplete %s due to the engine shutdown",
            m_queue.size(),
            m_queue.size() == 1 ? "session" : "sessions"
        );

        // Abort all the outstanding sessions.
        while(!m_queue.empty()) {
            m_queue.front()->abandon(
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
        if(it->second->state() == slave_t::states::active) {
            send(
                it->second->id(),
                io::message<rpc::terminate>()
            );

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

    if(m_state == states::stopping) {
        m_state = states::stopped;
        m_loop.unloop(ev::ALL);
    }
}
