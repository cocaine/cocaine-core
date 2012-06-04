//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include "cocaine/engine.hpp"

#include "cocaine/context.hpp"
#include "cocaine/drivers.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/helpers/archive_file.hpp"

using namespace cocaine::engine;
using namespace cocaine::networking;

// Job queue
// ---------

void job_queue_t::push(const_reference job) {
    if(job->policy.urgent) {
        push_front(job);
        job->process_event(events::enqueue(1));
    } else {
        push_back(job);
        job->process_event(events::enqueue(size()));
    }
}

// Basic stuff
// -----------

engine_t::engine_t(context_t& context, ev::loop_ref& loop, const std::string& name):
    m_context(context),
    m_log(m_context.log(name)),
    m_running(false),
    m_loop(loop),
    m_watcher(m_loop),
    m_processor(m_loop),
    m_pumper(m_loop),
    m_gc_timer(m_loop),
    m_bus(context.io(), ZMQ_ROUTER)
#ifdef HAVE_CGROUPS
    , m_cgroup(NULL)
#endif
{
    m_manifest.reset(new manifest_t(m_context, name));

#ifdef HAVE_CGROUPS
    Json::Value limits(manifest().root["resource-limits"]);

    if(!(cgroup_init() == 0) || !limits.isObject() || limits.empty()) {
        return;
    }
    
    m_cgroup = cgroup_new_cgroup(name.c_str());

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
                    log().error(
                        "controller '%s' parameter '%s' type is not supported",
                        c->c_str(),
                        p->c_str()
                    );

                    continue;
                }
            }
            
            log().debug(
                "setting controller '%s' parameter '%s' to %s", 
                c->c_str(),
                p->c_str(),
                boost::lexical_cast<std::string>(cfg[*p]).c_str()
            );
        }
    }

    int rv = 0;

    if((rv = cgroup_create_cgroup(m_cgroup, false)) != 0) {
        log().error(
            "unable to create the control group - %s", 
            cgroup_strerror(rv)
        );

        cgroup_free(&m_cgroup);
        m_cgroup = NULL;
    }
#endif
}

engine_t::~engine_t() {
    if(m_running) {
        stop();
    }

#ifdef HAVE_CGROUPS
    if(m_cgroup) {
        int rv = 0;

        // XXX: Sometimes there're still slaves terminating at this point,
        // so control group deletion fails with "Device or resource busy".
        if((rv = cgroup_delete_cgroup(m_cgroup, false)) != 0) {
            log().error(
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
    BOOST_ASSERT(!m_running);

    log().info("starting the engine"); 

    int linger = 0;

    m_bus.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    try {
        m_bus.bind(endpoint(manifest().name));
    } catch(const zmq::error_t& e) {
        throw configuration_error_t(std::string("invalid rpc endpoint - ") + e.what());
    }
    
    m_watcher.set<engine_t, &engine_t::message>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_processor.set<engine_t, &engine_t::process>(this);
    m_pumper.set<engine_t, &engine_t::pump>(this);
    m_pumper.start(0.005f, 0.005f);

    m_gc_timer.set<engine_t, &engine_t::cleanup>(this);
    m_gc_timer.start(5.0f, 5.0f);

    // Event configuration
    // -------------------

    // Json::Value events(m_app.manifest["tasks"]);

    // if(!events.isNull() && events.size()) {
    //     Json::Value::Members names(events.getMemberNames());

    //     m_app.log->info(
    //         "initializing drivers for %zu %s: %s",
    //         events.size(),
    //         events.size() == 1 ? "events" : "events",
    //         boost::algorithm::join(names, ", ").c_str()
    //     );
    
    //     for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
    //         std::string event(*it);
    //         std::string type(events[event]["type"].asString());
            
    //         if(type == "recurring-timer") {
    //             m_drivers.insert(event, new drivers::recurring_timer_t(*this, event, events[event]));
    //         } else if(type == "drifting-timer") {
    //             m_drivers.insert(event, new drivers::drifting_timer_t(*this, event, events[event]));
    //         } else if(type == "filesystem-monitor") {
    //             m_drivers.insert(event, new drivers::filesystem_monitor_t(*this, event, events[event]));
    //         } else if(type == "zeromq-server") {
    //             m_drivers.insert(event, new drivers::zeromq_server_t(*this, event, events[event]));
    //         } else if(type == "zeromq-sink") {
    //             m_drivers.insert(event, new drivers::zeromq_sink_t(*this, event, events[event]));
    //         } else if(type == "server+lsd" || type == "native-server") {
    //             m_drivers.insert(event, new drivers::native_server_t(*this, event, events[event]));
    //         } else {
    //            throw configuration_error_t("no driver for '" + type + "' is available");
    //         }
    //     }
    // } else {
    //     throw configuration_error_t("no events has been specified");
    // }

    m_running = true;
}

namespace {
    struct terminate {
        template<class T>
        void operator()(const T& master) {
            master->second->process_event(events::terminate());
        }
    };
}

void engine_t::stop() {
    BOOST_ASSERT(m_running);
    
    log().info("stopping the engine"); 

    {
        boost::lock_guard<boost::mutex> lock(m_queue_mutex);

        m_running = false;

        // Abort all the outstanding jobs.
        if(!m_queue.empty()) {
            log().debug(
                "dropping %zu queued %s",
                m_queue.size(),
                m_queue.size() == 1 ? "job" : "jobs"
            );

            while(!m_queue.empty()) {
                m_queue.front()->process_event(
                    events::error(
                        dealer::resource_error,
                        "engine is not active"
                    )
                );

                m_queue.pop_front();
            }
        }
    }

    rpc::packed<rpc::terminate> packed;

    // Send the termination event to active slaves.
    multicast(select::state<slave::alive>(), packed);

    // XXX: Might be a good idea to wait for a graceful termination.
    std::for_each(m_pool.begin(), m_pool.end(), terminate());

    m_pool.clear();
    // m_drivers.clear();

    m_watcher.stop();
    m_processor.stop();
    m_pumper.stop();
    m_gc_timer.stop();
}

Json::Value engine_t::info() {
    Json::Value results(Json::objectValue);

    if(m_running) {
        results["queue-depth"] = static_cast<Json::UInt>(m_queue.size());
        results["slaves"]["total"] = static_cast<Json::UInt>(m_pool.size());
        
        results["slaves"]["busy"] = static_cast<Json::UInt>(
            std::count_if(
                m_pool.begin(),
                m_pool.end(),
                select::state<slave::busy>()
            )
        );

        // for(driver_map_t::iterator it = m_drivers.begin();
        //     it != m_drivers.end();
        //     ++it) 
        // {
        //     results["tasks"][it->first] = it->second->info();
        // }
    }
    
    results["running"] = m_running;

    return results;
}

// Queue operations
// ----------------

void engine_t::enqueue(job_queue_t::const_reference job) {
    boost::lock_guard<boost::mutex> lock(m_queue_mutex);
    
    if(!m_running) {
        log().debug(
            "dropping a '%s' job",
            job->event.c_str()
        );

        job->process_event(
            events::error(
                dealer::resource_error,
                "engine is not active"
            )
        );

        return;
    }

    if(m_queue.size() >= manifest().policy.queue_limit) {
        job->process_event(
            events::error(
                dealer::resource_error,
                "the queue is full"
            )
        );
    } else {
        m_queue.push(job);
    }
}

void engine_t::process_queue() {
    boost::lock_guard<boost::mutex> lock(m_queue_mutex);

    if(m_queue.empty()) {
        return;
    }

    while(!m_queue.empty()) {
        job_queue_t::value_type job(m_queue.front());

        if(job->state_downcast<const job::complete*>()) {
            m_queue.pop_front();
            continue;
        }
            
        events::invoke event(job);

        rpc::packed<rpc::invoke> packed(
            job->event,
            job->request.data(),
            job->request.size()
        );

        pool_map_t::iterator it(
            unicast(
                select::state<slave::idle>(),
                packed
            )
        );

        // NOTE: If we got an idle slave, then we're lucky and got an instant scheduling;
        // if not, try to spawn more slaves, and enqueue the job.
        if(it != m_pool.end()) {
            it->second->process_event(event);
            m_queue.pop_front();
        } else {
            if(m_pool.empty() || 
              (m_pool.size() < manifest().policy.pool_limit && 
               m_pool.size() * manifest().policy.grow_threshold < m_queue.size() * 2))
            {
                std::auto_ptr<master_t> master;
                
                try {
                    master.reset(new master_t(m_context, *this));
                    std::string master_id(master->id());
                    m_pool.insert(master_id, master);
                } catch(const system_error_t& e) {
                    log().error(
                        "unable to spawn more slaves - %s - %s",
                        e.what(),
                        e.reason()
                    );
                }
            }

            break;
        }
    }
}

// Slave I/O
// ---------

void engine_t::message(ev::io&, int) {
    if(m_bus.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void engine_t::process(ev::idle&, int) {
    int counter = defaults::io_bulk_size;
    
    do {
        if(!m_bus.pending()) {
            m_processor.stop();
            return;
        }

        std::string slave_id;
        int command = 0;
        boost::tuple<raw<std::string>, int&> proxy(protect(slave_id), command);
                
        m_bus.recv_multi(proxy);

        pool_map_t::iterator master(m_pool.find(slave_id));

        if(master == m_pool.end()) {
            log().warning(
                "engine dropping type %d event from a nonexistent slave %s", 
                command,
                slave_id.c_str()
            );
            
            m_bus.drop();
            return;
        }

        switch(command) {
            case rpc::heartbeat:
                master->second->process_event(events::heartbeat());
                break;

            case rpc::chunk: {
                // TEST: Ensure we have the actual chunk following.
                BOOST_ASSERT(m_bus.more());

                zmq::message_t message;
                m_bus.recv(&message);
                
                master->second->process_event(events::chunk(message));

                break;
            }
         
            case rpc::error: {
                // TEST: Ensure that we have the actual error following.
                BOOST_ASSERT(m_bus.more());

                int code = 0;
                std::string message;
                boost::tuple<int&, std::string&> proxy(code, message);

                m_bus.recv_multi(proxy);

                master->second->process_event(events::error(code, message));

                if(code == dealer::server_error) {
                    log().error("the app seems to be broken: %s", message.c_str());
                    stop();
                }

                break;
            }

            case rpc::choke:
                master->second->process_event(events::choke());
                break;

            default:
                log().warning("engine dropping unknown event type %d", command);
                m_bus.drop();
        }

        // TEST: Ensure that there're no more message parts pending on the bus.
        BOOST_ASSERT(!m_bus.more());

        // NOTE: If we have an idle slave now, process the queue.
        if(master->second->state_downcast<const slave::idle*>()) {
            process_queue();
        }
    } while(--counter);
}

void engine_t::pump(ev::timer&, int) {
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

        log().debug(
            "recycled %zu dead %s", 
            corpses.size(),
            corpses.size() == 1 ? "slave" : "slaves"
        );
    }

    {
        typedef boost::filter_iterator<expired, job_queue_t::iterator> filter;

        boost::lock_guard<boost::mutex> lock(m_queue_mutex);
        
        // Process all the expired jobs.
        filter it(expired(m_loop.now()), m_queue.begin(), m_queue.end()),
               end(expired(m_loop.now()), m_queue.end(), m_queue.end());

        while(it != end) {
            (*it++)->process_event(
                events::error(
                    dealer::deadline_error,
                    "the job has expired"
                )
            );
        }
    }
}

// Threaded engine
// ---------------

threaded_engine_t::threaded_engine_t(context_t& context, const std::string& name):
    m_engine(context, m_loop, name),
    m_async_enqueue(m_loop)
{
    m_async_enqueue.set<threaded_engine_t, &threaded_engine_t::do_enqueue>(this);
    m_async_enqueue.start();
}

void threaded_engine_t::start() {
    m_thread.reset(
        new boost::thread(
            boost::bind(&threaded_engine_t::bootstrap, this)
        )
    );
}

Json::Value threaded_engine_t::info() {
    return m_engine.info();
}

void threaded_engine_t::enqueue(job_queue_t::const_reference job) {
    m_engine.enqueue(job);
    m_async_enqueue.send();
}

void threaded_engine_t::bootstrap() {
    m_engine.start();
    m_loop.loop();
}

void threaded_engine_t::do_enqueue(ev::async& w, int r) {
    m_engine.process_queue();
}
