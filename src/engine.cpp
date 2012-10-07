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
#include "cocaine/profile.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/traits/json.hpp"

using namespace cocaine::engine;

// Job queue
// ---------

void
job_queue_t::push(const_reference job) {
    if(job->policy.urgent) {
        emplace_front(job);
    } else {
        emplace_back(job);
    }
}

// Selectors
// ---------

namespace { namespace select {
    template<class State>
    struct state {
        template<class T>
        bool
        operator()(const T& master) const {
            return master.second->template state_downcast<const State*>();
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
// ------

engine_t::engine_t(context_t& context, const manifest_t& manifest, const profile_t& profile):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % manifest.name
        ).str()
    )),
    m_manifest(manifest),
    m_profile(profile),
    m_state(stopped),
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
    BOOST_ASSERT(m_state == stopped);
}

void
engine_t::run() {
    m_state = running;
    m_loop.loop();
}

bool
engine_t::enqueue(job_queue_t::const_reference job,
                  mode::value mode)
{
    if(m_state != running) {
        m_log->debug(
            "dropping an incomplete '%s' job due to an inactive engine",
            job->event.c_str()
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

    boost::unique_lock<boost::mutex> queue_lock(m_queue_mutex);

    if(m_queue.size() < m_profile.queue_limit) {
        m_queue.push(job);
    } else {
        switch(mode) {
            case mode::normal:
                // Release the lock, as it's not needed anymore.
                queue_lock.unlock();

                m_log->debug(
                    "dropping an incomplete '%s' job due to a full queue",
                    job->event.c_str()
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
                    m_queue_condition.wait(queue_lock);
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
            return job->policy.deadline && job->policy.deadline <= now;
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
        if(it->second->state_downcast<const slave::dead*>()) {
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

        m_log->debug(
            "recycled %zu dead %s", 
            corpses.size(),
            corpses.size() == 1 ? "slave" : "slaves"
        );
    }

    typedef boost::filter_iterator<
        expired_t,
        job_queue_t::iterator
    > filter_t;
    
    boost::unique_lock<boost::mutex> queue_lock(m_queue_mutex);

    // Process all the expired jobs.
    filter_t it(expired_t(m_loop.now()), m_queue.begin(), m_queue.end()),
             end(expired_t(m_loop.now()), m_queue.end(), m_queue.end());

    while(it != end) {
        boost::shared_ptr<job_t>& job(*it);

        m_log->debug(
            "the '%s' job has expired",
            job->event.c_str()
        );

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
    m_log->warning("forcing engine termination");
    clear();
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
    
        std::string slave_id;
        int command = -1;
        
        boost::tuple<
            io::raw<std::string>,
            int&
        > proxy(io::protect(slave_id), command);
        
        {
            // Try to read the next RPC command from the bus in a
            // non-blocking fashion. If it fails, break the loop.
            io::scoped_option<
                io::options::receive_timeout
            > option(*m_bus, 0);
            
            if(!m_bus->recv_tuple(proxy)) {
                break;            
            }
        }

        pool_map_t::iterator master(m_pool.find(slave_id));

        if(master == m_pool.end()) {
            m_log->warning(
                "dropping type %d message from an unknown slave %s", 
                command,
                slave_id.c_str()
            );
            
            m_bus->drop();

#ifdef ZMQ_ROUTER_BEHAVIOR
            // NOTE: Do a non-blocking send.
            io::scoped_option<
                io::options::send_timeout
            > option(*m_bus, 0);
            
            bool success = m_bus->send(io::protect(slave_id), ZMQ_SNDMORE) &&
                           m_bus->send_message(io::message<rpc::terminate>());
            
            if(!success) {
                m_log->warning(
                    "unable to kill an unknown slave %s", 
                    slave_id.c_str()
                );
            }
#else
            m_bus->send(io::protect(slave_id), ZMQ_SNDMORE);
            m_bus->send_message(io::message<rpc::terminate>());
#endif

            continue;
        }

        m_log->debug(
            "received type %d message from slave %s",
            command,
            slave_id.c_str()
        );

        switch(command) {
            case io::get<rpc::heartbeat>::value:
                master->second->process_event(events::heartbeat());
                break;

            case io::get<rpc::terminate>::value:
                if(master->second->state_downcast<const slave::busy*>()) {
                    // NOTE: Reschedule an incomplete job.
                    m_queue.push(master->second->state_downcast<const slave::alive*>()->job);
                }

                master->second->process_event(events::terminate());

                // Remove the dead slave from the pool.
                m_pool.erase(master);

                if(m_state == stopping && m_pool.empty()) {
                    // If it was the last slave, shut the engine down.
                    clear();
                    return;
                }

                continue;

            case io::get<rpc::chunk>::value: {
                zmq::message_t message;
                
                // TEST: Ensure we have the actual chunk following.
                BOOST_ASSERT(m_bus->more());

                m_bus->recv(&message);
                
                master->second->process_event(events::chunk(message));

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

                master->second->process_event(events::error(code, message));

                if(code == server_error) {
                    m_log->error("the app seems to be broken - %s", message.c_str());
                    shutdown();
                    return;
                }

                continue;
            }

            case io::get<rpc::choke>::value:
                master->second->process_event(events::choke());
                break;

            default:
                m_log->warning(
                    "dropping unknown type %d message from slave %s",
                    command,
                    slave_id.c_str()
                );

                m_bus->drop();
        }
    } while(--counter);
}

namespace {
    static const char * states[] = {
        "running",
        "stopping",
        "stopped"
    };
}

void
engine_t::process_ctl_events() {
    int command = -1;

    if(!m_ctl->recv(command)) {
        m_log->error("received a corrupted control message");
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
                    select::state<slave::busy>()
                )
            );

            info["state"] = states[m_state];

            m_ctl->send(info);

            break;
        }

        case io::get<control::terminate>::value:
            shutdown();
            break;

        default:
            m_log->error(
                "received an unknown control message, code: %d",
                command
            );

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
                select::state<slave::idle>()
            )
        );

        if(it != m_pool.end()) {
            job_queue_t::value_type job;

            {
                boost::unique_lock<boost::mutex> queue_lock(m_queue_mutex);

                if(m_queue.empty()) {
                    break;
                }

                job = m_queue.front();
                m_queue.pop_front();
            }

            // Notify one of the blocked enqueue operations.
            m_queue_condition.notify_one();
           
            {
                job_t::lock_t lock(*job);

                if(job->state == job_t::complete) {
                    m_log->debug(
                        "dropping a complete '%s' job from the queue",
                        job->event.c_str()
                    );

                    continue;
                }
            }
            
            io::message<rpc::invoke> message(
                job->event
            );

#ifdef ZMQ_ROUTER_BEHAVIOR
            if(send(*it->second, message)) {
                it->second->process_event(
                    events::invoke(job, it->second)
                );
                
                // TODO: Check if it helps.
                m_loop.feed_fd_event(m_bus->fd(), ev::READ);
            } else {
                m_log->error(
                    "slave %s has unexpectedly died",
                    it->first.c_str()
                );

                it->second->process_event(events::terminate());
                
                m_pool.erase(it);
                
                {
                    boost::unique_lock<boost::mutex> queue_lock(m_queue_mutex);
                    m_queue.emplace_front(job);
                }
            }
#else
            send(*it->second, message);
            
            it->second->process_event(
                events::invoke(job, it->second)
            );
                        
            // TODO: Check if it helps.
            m_loop.feed_fd_event(m_bus->fd(), ev::READ);
#endif
        } else {
            break;
        }
    }
}

void
engine_t::balance() {
    // NOTE: Balance the slave pool in order to keep it in a proper shape
    // based on the queue size and other policies.
    if(m_pool.size() >= m_profile.pool_limit ||
       m_pool.size() * m_profile.grow_threshold >= m_queue.size() * 2)
    {
        return;
    }

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

    m_log->info(
        "enlarging the pool from %d to %d slaves",
        m_pool.size(),
        target
    );

    while(m_pool.size() != target) {
        try {
            // Try to spawn a new slave process.
            boost::shared_ptr<master_t> master(
                boost::make_shared<master_t>(
                    m_context,
                    *this,
                    m_manifest,
                    m_profile
                )
            );
          
            m_pool.emplace(
                master->id(),
                master
            );
        } catch(const system_error_t& e) {
            m_log->error(
                "unable to spawn more slaves - %s - %s",
                e.what(),
                e.reason()
            );
        }
    }
}

void
engine_t::shutdown() {
    m_state = stopping;

    {
        boost::unique_lock<boost::mutex> queue_lock(m_queue_mutex);

        if(!m_queue.empty()) {
            m_log->debug(
                "dropping %zu incomplete %s due to the engine shutdown",
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
    }

    unsigned int pending = 0;

    {
        // Send the termination event to active slaves.
        // If there're no active slaves, the engine can terminate right away,
        // otherwise, the engine should wait for the specified timeout for slaves
        // to finish their jobs and then force the termination.
        for(pool_map_t::iterator it = m_pool.begin();
            it != m_pool.end();
            ++it)
        {
            if(it->second->state_downcast<const slave::alive*>()) {
                send(
                    *it->second,
                    io::message<rpc::terminate>()
                );

                ++pending;
            }
        }
    }

    if(pending) {
        m_log->info(
            "waiting for %d active %s to terminate, timeout: %.02f seconds",
            pending,
            pending == 1 ? "slave" : "slaves",
            m_profile.termination_timeout
        );
        
        m_termination_timer.set<engine_t, &engine_t::on_termination>(this);
        m_termination_timer.start(m_profile.termination_timeout);
    } else {
        clear();
    }    
}

namespace {
    struct terminate_t {
        template<class T>
        void
        operator()(const T& master) {
            master.second->process_event(events::terminate());
        }
    };
}

void
engine_t::clear() {
    // Force slave termination.
    std::for_each(
        m_pool.begin(),
        m_pool.end(),
        terminate_t()
    );
    
    m_pool.clear();
    m_loop.unloop(ev::ALL);
    
    m_state = stopped;
}
