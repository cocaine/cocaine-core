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

#include <boost/format.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include "cocaine/engine.hpp"

#include "cocaine/context.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/master.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/traits/json.hpp"
#include "cocaine/traits/unique_id.hpp"

using namespace cocaine::engine;

// Job queue

void
job_queue_t::push(const_reference job) {
    if(job->policy.urgent) {
        emplace_front(job);
    } else {
        emplace_back(job);
    }
}

// Selectors

namespace { namespace select {
    template<int State>
    struct state {
        template<class T>
        bool
        operator()(const T& master) const {
            return master.second->state == State;
        }
    };

    struct specific {
        specific(const master_t& master):
            target(master)
        { }

        template<class T>
        bool
        operator()(const T& master) const {
            return *master.second == target;
        }
    
        const master_t& target;
    };
}}

// Engine

engine_t::engine_t(context_t& context, const manifest_t& manifest, const profile_t& profile):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % manifest.name
        ).str()
    )),
    m_manifest(manifest),
    m_profile(profile),
    m_state(state::stopped),
    m_bus(new io::channel_t(context, ZMQ_ROUTER, m_manifest.name)),
    m_ctl(new io::channel_t(context, ZMQ_PAIR)),
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
        api::category_traits<api::isolate_t>::args_type(
            m_manifest.name,
            m_profile.isolate.args
        )
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
        boost::format message("unable to bind the engine pool channel - %s");
        throw configuration_error_t((message % e.what()).str());
    }
   
    std::string ctl_endpoint(
        (boost::format("inproc://%s")
            % m_manifest.name
        ).str()
    );

    try {
        m_ctl->connect(ctl_endpoint);
    } catch(const zmq::error_t& e) {
        boost::format message("unable to connect to the engine control channel - %s");
        throw configuration_error_t((message % e.what()).str());
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
    BOOST_ASSERT(m_state == state::stopped);
}

void
engine_t::run() {
    m_state = state::running;
    m_loop.loop();
}

bool
engine_t::enqueue(job_queue_t::const_reference job,
                  mode::value mode)
{
    if(m_state != state::running) {
        COCAINE_LOG_DEBUG(
            m_log,
            "dropping an incomplete '%s' job due to an inactive engine",
            job->event
        );

        job->process(
            events::error(
                resource_error,
                "engine is not active"
            )
        );

        job->process(events::choke());

        return false;
    }

    boost::unique_lock<boost::mutex> lock(m_queue_mutex);

    if(m_queue.size() < m_profile.queue_limit) {
        m_queue.push(job);
    } else {
        switch(mode) {
            case mode::normal:
                // NOTE: Unlock the queue mutex here so that the job error
                // processing would happen without it in order to let the
                // engine continue to function.
                lock.unlock();

                COCAINE_LOG_DEBUG(
                    m_log,
                    "dropping an incomplete '%s' job due to a full queue",
                    job->event
                );

                job->process(
                    events::error(
                        resource_error,
                        "the queue is full"
                    )
                );

                job->process(events::choke());
                
                return false;

            case mode::blocking:
                while(m_queue.size() >= m_profile.queue_limit) {
                    m_queue_condition.wait(lock);
                }

                m_queue.push(job);
        }
    }
  
    // Pump the queue! 
    m_notification.send();

    return true;
}

void
engine_t::on_bus_event(ev::io&, int) {
    if(m_bus->pending()) {
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

namespace {
    struct expired_t {
        expired_t(ev::tstamp now_):
            now(now_)
        { }

        template<class T>
        bool
        operator()(const T& job) {
            return job->policy.deadline &&
                   job->policy.deadline <= now;
        }

        ev::tstamp now;
    };
}

void
engine_t::on_cleanup(ev::timer&, int) {
    typedef std::vector<
        pool_map_t::key_type
    > corpse_list_t;
    
    corpse_list_t corpses;

    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        if(it->second->state == master_t::state::dead) {
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

    typedef boost::filter_iterator<
        expired_t,
        job_queue_t::iterator
    > filter_t;
    
    boost::unique_lock<boost::mutex> lock(m_queue_mutex);

    filter_t it(expired_t(m_loop.now()), m_queue.begin(), m_queue.end()),
             end(expired_t(m_loop.now()), m_queue.end(), m_queue.end());

    while(it != end) {
        boost::shared_ptr<job_t>& job(*it);

        COCAINE_LOG_DEBUG(m_log, "the '%s' job has expired", job->event);

        job->process(
            events::error(
                deadline_error,
                "the job has expired"
            )
        );
        
        job->process(events::choke());

        ++it;
    }
}

void
engine_t::on_termination(ev::timer&, int) {
    COCAINE_LOG_WARNING(m_log, "forcing the engine termination");
    stop();
}

void
engine_t::on_notification(ev::async&, int) {
    pump();
    balance();
}

void
engine_t::process_bus_events() {
    // NOTE: Try to read RPC calls in bulk, where the maximum size
    // of the bulk is proportional to the number of spawned slaves.
    int counter = m_pool.size() * defaults::io_bulk_size;
    
    do {
        // TEST: Ensure that we haven't missed something in a previous iteration.
        BOOST_ASSERT(!m_bus->more());
    
        unique_id_t slave_id(uninitialized);
        int command = -1;
        
        boost::tuple<
            unique_id_t&,
            int&
        > proxy(slave_id, command);
        
        {
            io::scoped_option<
                io::options::receive_timeout
            > option(*m_bus, 0);
            
            if(!m_bus->recv_tuple(proxy)) {
                break;            
            }
        }

        pool_map_t::iterator master(m_pool.find(slave_id));

        if(master == m_pool.end()) {
            COCAINE_LOG_WARNING(
                m_log,
                "dropping type %d message from an unknown slave %s", 
                command,
                slave_id.string()
            );
            
            m_bus->drop();
            
            // NOTE: Trying to kill the orphaned slave, just in case.
            // For some reason, this doesn't work, so a new approach
            // is needed to get rid of such stuff.
            
            send(
                slave_id,
                io::message<rpc::terminate>()
            );

            continue;
        }

        COCAINE_LOG_DEBUG(
            m_log,
            "received type %d message from slave %s",
            command,
            slave_id.string()
        );

        switch(command) {
            case io::get<rpc::heartbeat>::value:
                master->second->process(events::heartbeat());
                break;

            case io::get<rpc::terminate>::value:
                if(master->second->state == master_t::state::busy) {
                    // NOTE: Reschedule an incomplete job.
                    m_queue.push(master->second->job);
                }

                master->second->process(events::terminate());

                // Remove the dead slave from the pool.
                m_pool.erase(master);

                if(m_state != state::running && m_pool.empty()) {
                    // If it was the last slave, shut the engine down.
                    stop();
                    return;
                }

                continue;

            case io::get<rpc::chunk>::value: {
                zmq::message_t message;
                
                // TEST: Ensure we have the actual chunk following.
                BOOST_ASSERT(m_bus->more());

                m_bus->recv(&message);
                
                master->second->process(events::chunk(message));

                continue;
            }
         
            case io::get<rpc::error>::value: {
                int code = 0;
                std::string message;

                boost::tuple<
                    int&,
                    std::string&
                > proxy(code, message);

                // TEST: Ensure that we have the actual error following.
                BOOST_ASSERT(m_bus->more());

                m_bus->recv_tuple(proxy);

                master->second->process(events::error(code, message));

                if(code == server_error) {
                    COCAINE_LOG_ERROR(m_log, "the app seems to be broken - %s", message);
                    m_state = state::broken;
                    shutdown();
                    return;
                }

                continue;
            }

            case io::get<rpc::choke>::value:
                master->second->process(events::choke());
                break;

            default:
                COCAINE_LOG_WARNING(
                    m_log,
                    "dropping unknown type %d message from slave %s",
                    command,
                    slave_id.string()
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

void
engine_t::process_ctl_events() {
    int command = -1;

    if(!m_ctl->recv(command)) {
        COCAINE_LOG_ERROR(m_log, "received a corrupted control message");
        m_ctl->drop();
        return;
    }

    switch(command) {
        case io::get<control::status>::value: {
            Json::Value info(Json::objectValue);

            info["queue-depth"] = static_cast<Json::LargestUInt>(m_queue.size());

            info["slaves"]["total"] = static_cast<Json::LargestUInt>(m_pool.size());

            info["slaves"]["busy"] = static_cast<Json::LargestUInt>(
                std::count_if(
                    m_pool.begin(),
                    m_pool.end(),
                    select::state<master_t::state::busy>()
                )
            );

            info["state"] = describe[m_state];

            m_ctl->send(info);

            break;
        }

        case io::get<control::terminate>::value:
            m_state = state::stopping;
            shutdown();
            break;

        default:
            COCAINE_LOG_ERROR(m_log, "received an unknown control message, code: %d", command);
            m_ctl->drop();
    }
}

void
engine_t::pump() {
    while(!m_queue.empty()) {
        // NOTE: If we got an idle slave, then we're lucky and got an instant scheduling;
        // if not, try to spawn more slaves and wait.
        pool_map_t::iterator it(
            std::find_if(
                m_pool.begin(),
                m_pool.end(),
                select::state<master_t::state::idle>()
            )
        );

        if(it != m_pool.end()) {
            job_queue_t::value_type job;

            {
                boost::unique_lock<boost::mutex> lock(m_queue_mutex);

                if(m_queue.empty()) {
                    break;
                }

                job = m_queue.front();
                m_queue.pop_front();
            }

            // Notify one of the blocked enqueue operations.
            m_queue_condition.notify_one();
           
            if(job->state == job_t::state::complete) {
                COCAINE_LOG_DEBUG(m_log, "dropping a complete '%s' job from the queue", job->event);
                continue;
            }
            
            io::message<rpc::invoke> message(
                job->event
            );

            if(send(it->second->id(), message)) {
                it->second->process(
                    events::invoke(job, this, it->second)
                );
                
                // TODO: Check if it helps.
                m_loop.feed_fd_event(m_bus->fd(), ev::READ);
            } else {
                COCAINE_LOG_ERROR(m_log, "slave %s has unexpectedly died", it->first.string());
                
                it->second->process(events::terminate());
                m_pool.erase(it);
                
                {
                    boost::unique_lock<boost::mutex> lock(m_queue_mutex);
                    m_queue.emplace_front(job);
                }
            }
        } else {
            break;
        }
    }
}

void
engine_t::balance() {
    if(m_pool.size() >= m_profile.pool_limit ||
       m_pool.size() * m_profile.grow_threshold >= m_queue.size() * 2)
    {
        return;
    }

    // NOTE: Balance the slave pool in order to keep it in a proper shape
    // based on the queue size and other policies.
    
    unsigned int target = std::min(
        m_profile.pool_limit,
        std::max(
            2UL * m_queue.size() / m_profile.grow_threshold, 
            1UL
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
            boost::shared_ptr<master_t> master(
                boost::make_shared<master_t>(
                    m_context,
                    m_loop,
                    m_manifest,
                    m_profile
                )
            );
          
            m_pool.emplace(master->id(), master);
        } catch(const system_error_t& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to spawn more slaves - %s - %s",
                e.what(),
                e.reason()
            );
        }
    }
}

void
engine_t::shutdown() {
    boost::unique_lock<boost::mutex> lock(m_queue_mutex);

    if(!m_queue.empty()) {
        COCAINE_LOG_DEBUG(
            m_log,
            "dropping %llu incomplete %s due to the engine shutdown",
            m_queue.size(),
            m_queue.size() == 1 ? "job" : "jobs"
        );

        // Abort all the outstanding jobs.
        while(!m_queue.empty()) {
            m_queue.front()->process(
                events::error(
                    resource_error,
                    "engine is shutting down"
                )
            );

            m_queue.front()->process(events::choke());
            m_queue.pop_front();
        }
    }

    unsigned int pending = 0;

    // NOTE: Send the termination event to active slaves.
    // If there're no active slaves, the engine can terminate right away,
    // otherwise, the engine should wait for the specified timeout for slaves
    // to finish their jobs and then force the termination.
    
    for(pool_map_t::iterator it = m_pool.begin();
        it != m_pool.end();
        ++it)
    {
        if(it->second->state == master_t::state::idle ||
           it->second->state == master_t::state::busy)
        {
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

namespace {
    struct terminate_t {
        template<class T>
        void
        operator()(const T& master) {
            master.second->process(events::terminate());
        }
    };
}

void
engine_t::stop() {
    m_termination_timer.stop();

    // Force the slave termination.
    std::for_each(m_pool.begin(), m_pool.end(), terminate_t());
    m_pool.clear();

    if(m_state == state::stopping) {
        m_state = state::stopped;
        m_loop.unloop(ev::ALL);
    }
}
