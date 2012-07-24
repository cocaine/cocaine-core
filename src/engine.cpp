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

#include <boost/assign.hpp>
#include <boost/format.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/lexical_cast.hpp>

#include "cocaine/engine.hpp"

#include "cocaine/context.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/rpc.hpp"

using namespace cocaine::engine;

void job_queue_t::push(const_reference job) {
    if(job->policy.urgent) {
        push_front(job);
        job->process_event(events::enqueue(1));
    } else {
        push_back(job);
        job->process_event(events::enqueue(size()));
    }
}

engine_t::engine_t(context_t& context, const manifest_t& manifest):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % manifest.name
        ).str()
    )),
    m_state(stopped),
    m_manifest(manifest),
    m_watcher(m_loop),
    m_processor(m_loop),
    m_check(m_loop),
    m_gc_timer(m_loop),
    m_termination_timer(m_loop),
    m_notification(m_loop),
    m_bus(context.io(), m_manifest.name),
    m_thread(NULL)
#ifdef HAVE_CGROUPS
    , m_cgroup(NULL)
#endif
{
    int linger = 0;

    m_bus.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    std::string endpoint(
        (boost::format("ipc://%1%/%2%")
            % m_context.config.ipc_path
            % m_manifest.name
        ).str()
    );

    try {
        m_bus.bind(endpoint);
    } catch(const zmq::error_t& e) {
        throw configuration_error_t(std::string("invalid rpc endpoint - ") + e.what());
    }
    
    m_watcher.set<engine_t, &engine_t::message>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_processor.set<engine_t, &engine_t::process>(this);
    m_check.set<engine_t, &engine_t::check>(this);
    m_check.start();

    m_gc_timer.set<engine_t, &engine_t::cleanup>(this);
    m_gc_timer.start(5.0f, 5.0f);

    m_notification.set<engine_t, &engine_t::notify>(this);
    m_notification.start();

#ifdef HAVE_CGROUPS
    Json::Value limits(m_manifest.root["resource-limits"]);

    if(!(cgroup_init() == 0) || !limits.isObject() || limits.empty()) {
        return;
    }
    
    m_cgroup = cgroup_new_cgroup(m_manifest.name.c_str());

    // XXX: Not sure if it changes anything.
    cgroup_set_uid_gid(m_cgroup, getuid(), getgid(), getuid(), getgid());
    
    Json::Value::Members controllers(limits.getMemberNames());

    for(Json::Value::Members::iterator c = controllers.begin();
        c != controllers.end();
        ++c)
    {
        Json::Value cfg(limits[*c]);

        if(!cfg.isObject() || cfg.empty()) {
            continue;
        }
        
        cgroup_controller * ctl = cgroup_add_controller(m_cgroup, c->c_str());
        
        Json::Value::Members parameters(cfg.getMemberNames());

        for(Json::Value::Members::iterator p = parameters.begin();
            p != parameters.end();
            ++p)
        {
            switch(cfg[*p].type()) {
                case Json::stringValue: {
                    cgroup_add_value_string(ctl, p->c_str(), cfg[*p].asCString());
                    break;
                } case Json::intValue: {
                    cgroup_add_value_int64(ctl, p->c_str(), cfg[*p].asInt());
                    break;
                } case Json::uintValue: {
                    cgroup_add_value_uint64(ctl, p->c_str(), cfg[*p].asUInt());
                    break;
                } case Json::booleanValue: {
                    cgroup_add_value_bool(ctl, p->c_str(), cfg[*p].asBool());
                    break;
                } default: {
                    m_log->error(
                        "controller '%s' parameter '%s' type is not supported",
                        c->c_str(),
                        p->c_str()
                    );

                    continue;
                }
            }
            
            m_log->debug(
                "setting controller '%s' parameter '%s' to '%s'", 
                c->c_str(),
                p->c_str(),
                boost::lexical_cast<std::string>(cfg[*p]).c_str()
            );
        }
    }

    int rv = 0;

    if((rv = cgroup_create_cgroup(m_cgroup, false)) != 0) {
        m_log->error(
            "unable to create the control group - %s", 
            cgroup_strerror(rv)
        );

        cgroup_free(&m_cgroup);
        m_cgroup = NULL;
    }
#endif
}

engine_t::~engine_t() {
    {
        // LOCK: Obtain a shared lock to block state changes.
        boost::shared_lock<boost::shared_mutex> lock(m_mutex);
        BOOST_VERIFY(m_state == stopped);
    }

#ifdef HAVE_CGROUPS
    if(m_cgroup) {
        int rv = 0;

        // FIXME: Sometimes there're still slaves terminating at this point,
        // so control group deletion fails with "Device or resource busy".
        if((rv = cgroup_delete_cgroup(m_cgroup, false)) != 0) {
            m_log->error(
                "unable to delete the control group - %s", 
                cgroup_strerror(rv)
            );
        }

        cgroup_free(&m_cgroup);
    }
#endif
}

// Operations
// ----------

void engine_t::start() {
    {
        // LOCK: Obtain an unique lock to block state changes.
        boost::unique_lock<boost::shared_mutex> lock(m_mutex);
        
        if(m_state != stopped) {
            return;
        }

        m_state = running;
    }

    m_log->info("starting");
    
    m_thread.reset(
        new boost::thread(
            boost::bind(
                &ev::dynamic_loop::loop,
                boost::ref(m_loop),
                0
            )
        )
    );
}

void engine_t::stop() {
    {
        // LOCK: Obtain an unique lock to block state changes.
        boost::unique_lock<boost::shared_mutex> lock(m_mutex);

        if(m_state == running) {
            m_log->info("stopping");

            shutdown();
        }
    }

    if(m_thread.get()) {
        m_log->debug("waiting for the engine to terminate");
        
        // Wait for the termination.
        m_thread->join();
        m_thread.reset();
    }
}

namespace {
    static std::map<int, std::string> state_names = boost::assign::map_list_of
        (0, "running")
        (1, "stopping")
        (2, "stopped");
}

Json::Value engine_t::info() const {
    Json::Value results(Json::objectValue);

    // LOCK: Obtain a shared lock to block state changes.
    boost::shared_lock<boost::shared_mutex> lock(m_mutex);

    if(m_state == running) {
        results["queue-depth"] = static_cast<Json::UInt>(m_queue.size());
        results["slaves"]["total"] = static_cast<Json::UInt>(m_pool.size());
        
        results["slaves"]["busy"] = static_cast<Json::UInt>(
            std::count_if(
                m_pool.begin(),
                m_pool.end(),
                select::state<slave::busy>()
            )
        );
    }
    
    results["state"] = state_names[m_state];

    return results;
}

// Job scheduling
// --------------

bool engine_t::enqueue(job_queue_t::const_reference job) {
    // LOCK: Obtain an upgradable lock to block state changes.
    boost::unique_lock<boost::shared_mutex> lock(m_mutex);

    if(m_state != running) {
        m_log->debug(
            "dropping an incomplete '%s' job due to an inactive engine",
            job->event.c_str()
        );

        job->process_event(
            events::error(
                resource_error,
                "engine is not active"
            )
        );

        return false;
    }

    if(m_queue.size() >= m_manifest.policy.queue_limit) {
        m_log->debug(
            "dropping an incomplete '%s' job due to a full queue",
            job->event.c_str()
        );

        job->process_event(
            events::error(
                resource_error,
                "the queue is full"
            )
        );

        return false;
    }

    m_queue.push(job);
    m_notification.send();
    
    return true;
}

// Slave I/O
// ---------

void engine_t::message(ev::io&, int) {
    if(m_bus.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void engine_t::process(ev::idle&, int) {
    int counter = 0;

    {
        // LOCK: Obtain a shared lock to block pool changes.
        boost::shared_lock<boost::shared_mutex> lock(m_mutex);

        // NOTE: Try to read RPC calls in bulk, where the maximum size
        // of the bulk is proportional to the number of spawned slaves.
        counter = m_pool.size() * defaults::io_bulk_size;
    }
    
    do {
        // LOCK: Obtain an upgradable lock to block bus and pool changes.
        boost::unique_lock<boost::shared_mutex> lock(m_mutex);

        // TEST: Ensure that we haven't missed something in a previous iteration.
        BOOST_ASSERT(!m_bus.more());
    
        std::string slave_id;
        int command = 0;
        
        boost::tuple<
            io::raw<std::string>,
            int&
        > proxy(io::protect(slave_id), command);
        
        {
            io::scoped_option<io::options::receive_timeout> option(m_bus, 0);
         
            // Try to read the next RPC command from the bus in a
            // non-blocking fashion. If it fails, break the loop.
            if(!m_bus.recv_multi(proxy)) {
                m_processor.stop();
                return;            
            }
        }

        pool_map_t::iterator master(m_pool.find(slave_id));

        if(master == m_pool.end()) {
            m_log->warning(
                "dropping type %d event from an unknown slave %s", 
                command,
                slave_id.c_str()
            );
            
            m_bus.drop();

            io::scoped_option<io::options::send_timeout> option(m_bus, 0);

            // Try to kill the unknown slave, just in case.
            // FIXME: This doesn't work for some reason.

            m_bus.send(
                slave_id,
                io::packed<rpc::domain, rpc::terminate>()
            );

            continue;
        }

        m_log->debug(
            "got type %d event from slave %s",
            command,
            slave_id.c_str()
        );

        switch(command) {
            case rpc::heartbeat:
                master->second->process_event(events::heartbeat());
                break;

            case rpc::terminate:
                if(master->second->state_downcast<const slave::busy*>()) {
                    // NOTE: Reschedule an incomplete job.
                    m_queue.push(master->second->state_downcast<const slave::alive*>()->job);
                }

                master->second->process_event(events::terminate());

                // Remove the dead slave from the pool.
                m_pool.erase(master);

                if(m_state == stopping && m_pool.empty()) {
                    // If it was the last slave, shut the engine down.
                    m_state = stopped;
                    m_notification.send();
                    return;
                }

                continue;

            case rpc::chunk: {
                // TEST: Ensure we have the actual chunk following.
                BOOST_ASSERT(m_bus.more());

                zmq::message_t message;
                m_bus.recv(&message);
                
                master->second->process_event(events::chunk(message));

                continue;
            }
         
            case rpc::error: {
                // TEST: Ensure that we have the actual error following.
                BOOST_ASSERT(m_bus.more());

                int code = 0;
                std::string message;
                boost::tuple<int&, std::string&> proxy(code, message);

                m_bus.recv_multi(proxy);

                master->second->process_event(events::error(code, message));

                if(code == server_error) {
                    m_log->error("the app seems to be broken - %s", message.c_str());
                    shutdown();
                    return;
                }

                continue;
            }

            case rpc::choke:
                master->second->process_event(events::choke());
                break;

            default:
                m_log->warning(
                    "dropping unknown event type %d from slave %s",
                    command,
                    slave_id.c_str()
                );

                m_bus.drop();
        }

        if(master->second->state_downcast<const slave::idle*>()) {
            pump();
        }
    } while(--counter);
}

void engine_t::check(ev::prepare&, int) {
    message(m_watcher, ev::READ);
}

// Garbage collection
// ------------------

namespace {
    struct expired {
        expired(ev::tstamp now_):
            now(now_)
        { }

        template<class T>
        bool operator()(const T& job) {
            return job->policy.deadline && job->policy.deadline <= now;
        }

        ev::tstamp now;
    };
}

void engine_t::cleanup(ev::timer&, int) {
    // LOCK: Obtain an unique lock for pool observation.
    boost::unique_lock<boost::shared_mutex> lock(m_mutex);
    
    typedef std::vector<pool_map_t::key_type> corpse_list_t;
    corpse_list_t corpses;

    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        if(it->second->state_downcast<const slave::dead*>()) {
            corpses.push_back(it->first);
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

    // LOCK: Looks like the exclusive lock is not needed here as we don't
    // change the queue itself, but the jobs in it.

    typedef boost::filter_iterator<expired, job_queue_t::iterator> filter;

    // Process all the expired jobs.
    filter it(expired(m_loop.now()), m_queue.begin(), m_queue.end()),
           end(expired(m_loop.now()), m_queue.end(), m_queue.end());

    while(it != end) {
        (*it++)->process_event(
            events::error(
                deadline_error,
                "the job has expired"
            )
        );
    }
}

// Forced engine termination
// -------------------------

void engine_t::terminate(ev::timer&, int) {
    // LOCK: Obtain an exclusive lock for state operations.
    boost::unique_lock<boost::shared_mutex> lock(m_mutex);

    m_log->info("forcing engine termination");

    // Force slave termination.
    m_state = stopped;
    m_notification.send();
}

// Asynchronous notification
// -------------------------

namespace {
    struct terminator {
        template<class T>
        void operator()(const T& master) {
            master->second->process_event(events::terminate());
        }
    };
}

void engine_t::notify(ev::async&, int) {
    // LOCK: Obtain an exclusive lock for state operations.
    boost::unique_lock<boost::shared_mutex> lock(m_mutex);

    switch(m_state) {
        case running:
            pump();
            break;
        
        case stopping:
            m_termination_timer.set<engine_t, &engine_t::terminate>(this);
            m_termination_timer.start(m_manifest.policy.termination_timeout); 
            break;

        case stopped:
            std::for_each(m_pool.begin(), m_pool.end(), terminator());
            m_loop.unloop(ev::ALL);
            break;
    };
}

// Queue processing
// ----------------

void engine_t::pump() {
    while(!m_queue.empty()) {
        job_queue_t::value_type job(m_queue.front());

        if(job->state_downcast<const job::complete*>()) {
            m_log->debug(
                "dropping a complete '%s' job from the queue",
                job->event.c_str()
            );

            m_queue.pop_front();

            continue;
        }
            
        io::packed<rpc::domain, rpc::invoke> command(
            job->event,
            job->request.data(),
            job->request.size()
        );

        pool_map_t::iterator it(
            call(
                select::state<slave::idle>(),
                command
            )
        );

        // NOTE: If we got an idle slave, then we're lucky and got an instant scheduling;
        // if not, try to spawn more slaves, and enqueue the job.
        if(it != m_pool.end()) {
            it->second->process_event(events::invoke(job));
            m_queue.pop_front();
        } else {
            if(m_pool.empty() || 
              (m_pool.size() < m_manifest.policy.pool_limit && 
               m_pool.size() * m_manifest.policy.grow_threshold < m_queue.size() * 2))
            {
                int target = std::min(
                    m_manifest.policy.pool_limit,
                    m_manifest.policy.grow_threshold ? 
                        m_queue.size() * (2 / m_manifest.policy.grow_threshold) :
                        m_manifest.policy.pool_limit
                );
                
                m_log->debug(
                    "enlarging the pool from %d to %d slaves",
                    m_pool.size(),
                    target
                );
                
                while(m_pool.size() != target) {
                    std::auto_ptr<master_t> master;
                    
                    try {
                        master.reset(new master_t(m_context, *this));
                        std::string master_id(master->id());
                        m_pool.insert(master_id, master);
                    } catch(const system_error_t& e) {
                        m_log->error(
                            "unable to spawn more slaves - %s - %s",
                            e.what(),
                            e.reason()
                        );

                        return;
                    }
                }
            }

            return;
        }
    }
}

// Engine termination
// ------------------

void engine_t::shutdown() {
    if(!m_queue.empty()) {
        m_log->debug(
            "dropping %zu incomplete %s due to the engine shutdown",
            m_queue.size(),
            m_queue.size() == 1 ? "job" : "jobs"
        );

        // Abort all the outstanding jobs.
        while(!m_queue.empty()) {
            m_queue.front()->process_event(
                events::error(
                    resource_error,
                    "engine is shutting down"
                )
            );

            m_queue.pop_front();
        }
    }

    unsigned int pending = 0;

    // Send the termination event to active slaves.
    // If there're no active slaves, the engine can terminate right away,
    // otherwise, the engine should wait for the specified timeout for slaves
    // to finish their jobs and then force the termination.

    for(pool_map_t::iterator it = m_pool.begin();
        it != m_pool.end();
        ++it)
    {
        if(it->second->state_downcast<const slave::alive*>()) {
            call(
                select::specific(*it->second),
                io::packed<rpc::domain, rpc::terminate>()
            );

            ++pending;
        }
    }

    if(pending) {
        m_state = stopping;

        m_log->info(
            "waiting for %d active %s to terminate, timeout: %.02f seconds",
            pending,
            pending == 1 ? "slave" : "slaves",
            m_manifest.policy.termination_timeout
        );
    } else {
        m_state = stopped;
    }    
    
    m_notification.send();
}

