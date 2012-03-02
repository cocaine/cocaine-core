//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include "cocaine/drivers.hpp"

using namespace cocaine::engine;
using namespace cocaine::networking;

// Job queue
// ---------

void engine_t::job_queue_t::push(const_reference job) {
    if(job->policy().urgent) {
        push_front(job);
        job->process_event(events::enqueue_t(1));
    } else {
        push_back(job);
        job->process_event(events::enqueue_t(size()));
    }
}

// Selectors
// ---------

bool engine_t::idle_slave::operator()(pool_map_t::pointer slave) const {
    return slave->second->state_downcast<const slaves::idle*>();
}

engine_t::specific_slave::specific_slave(pool_map_t::pointer target):
    m_target(target),
    m_dead(false)
{
    if(m_target->second->state_downcast<const slaves::dead*>()) {
        m_dead = true;
    }
}

bool engine_t::specific_slave::operator()(pool_map_t::pointer slave) const {
    return !m_dead && slave == m_target;
}

namespace {
    struct busy_slave {
        bool operator()(engine_t::pool_map_t::const_pointer slave) const {
            return slave->second->state_downcast<const slaves::busy*>();
        }
    };
}

// Basic stuff
// -----------

engine_t::engine_t(context_t& ctx, const std::string& name, const Json::Value& manifest):
    object_t(ctx, name + " engine"),
    m_running(false),
    m_app(ctx, name, manifest),
    m_messages(ctx, ZMQ_ROUTER, "rpc/" + name)
#ifdef HAVE_CGROUPS
    , m_cgroup(NULL)
#endif
{
#ifdef HAVE_CGROUPS
    if(ctx.config.core.cgroups) {
        Json::Value limits(manifest["engine"]["resource-limits"]);

        if(limits.isObject() && !limits.empty()) {
            m_cgroup = cgroup_new_cgroup(name.c_str());

            Json::Value::Members ctl_names(limits.getMemberNames());

            for(Json::Value::Members::iterator c = ctl_names.begin();
                c != ctl_names.end();
                ++c)
            {
                Json::Value ctl_cfg(limits[*c]);

                if(ctl_cfg.isObject() && !ctl_cfg.empty()) {
                    cgroup_controller* ctl = cgroup_add_controller(m_cgroup, c->c_str());

                    Json::Value::Members parameter_names(ctl_cfg.getMemberNames());

                    for(Json::Value::Members::iterator p = parameter_names.begin();
                        p != parameter_names.end();
                        ++p)
                    {
                        switch(ctl_cfg[*p].type()) {
                            case Json::stringValue: {
                                cgroup_add_value_string(ctl, p->c_str(), ctl_cfg[*p].asCString());
                                break;
                            } case Json::intValue: {
                                cgroup_add_value_int64(ctl, p->c_str(), ctl_cfg[*p].asInt());
                                break;
                            } case Json::uintValue: {
                                cgroup_add_value_uint64(ctl, p->c_str(), ctl_cfg[*p].asUInt());
                                break;
                            } case Json::booleanValue: {
                                cgroup_add_value_bool(ctl, p->c_str(), ctl_cfg[*p].asBool());
                                break;
                            } default: {
                                log().error(
                                    "cgroup controller '%s' parameter '%s' type is not supported",
                                    c->c_str(),
                                    p->c_str()
                                );

                                continue;
                            }
                        }
                        
                        log().debug(
                            "setting cgroup controller '%s' parameter '%s' to %s", 
                            c->c_str(),
                            p->c_str(),
                            boost::lexical_cast<std::string>(ctl_cfg[*p]).c_str()
                        );
                    }
                }
            }

            int rv = 0;

            if((rv = cgroup_create_cgroup(m_cgroup, false)) != 0) {
                log().error(
                    "unable to create cgroup - %s", 
                    cgroup_strerror(rv)
                );

                cgroup_free(&m_cgroup);
                m_cgroup = NULL;
            }
        }
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

Json::Value engine_t::start() {
    BOOST_ASSERT(!m_running);

    log().info("starting"); 

    int linger = 0;

    m_messages.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    try {
        m_messages.bind(m_app.endpoint);
    } catch(const zmq::error_t& e) {
        throw std::runtime_error(std::string("rpc failure - ") + e.what());
    }
    
    m_watcher.set<engine_t, &engine_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<engine_t, &engine_t::process>(this);
    m_pumper.set<engine_t, &engine_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);

    m_gc_timer.set<engine_t, &engine_t::cleanup>(this);
    m_gc_timer.start(5.0f, 5.0f);
    
    // Tasks configuration
    // -------------------

    Json::Value tasks(m_app.manifest["tasks"]);

    if(!tasks.isNull() && tasks.size()) {
        Json::Value::Members names(tasks.getMemberNames());

        log().info(
            "initializing drivers for %zu %s: %s",
            tasks.size(),
            tasks.size() == 1 ? "task" : "tasks",
            boost::algorithm::join(names, ", ").c_str()
        );
    
        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            std::string task(*it);
            std::string type(tasks[task]["type"].asString());
            
            if(type == "recurring-timer") {
                m_tasks.insert(task, new drivers::recurring_timer_t(*this, task, tasks[task]));
            } else if(type == "drifting-timer") {
                m_tasks.insert(task, new drivers::drifting_timer_t(*this, task, tasks[task]));
            } else if(type == "filesystem-monitor") {
                m_tasks.insert(task, new drivers::filesystem_monitor_t(*this, task, tasks[task]));
            } else if(type == "zeromq-server") {
                m_tasks.insert(task, new drivers::zeromq_server_t(*this, task, tasks[task]));
            } else if(type == "zeromq-sink") {
                m_tasks.insert(task, new drivers::zeromq_sink_t(*this, task, tasks[task]));
            } else if(type == "server+lsd") {
                m_tasks.insert(task, new drivers::lsd_server_t(*this, task, tasks[task]));
            } else {
               throw std::runtime_error("no driver for '" + type + "' is available");
            }
        }
    } else {
        throw std::runtime_error("no tasks has been specified");
    }

    m_running = true;

    return info();
}

Json::Value engine_t::stop() {
    BOOST_ASSERT(m_running);
    
    log().info("stopping"); 

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
                events::error_t(
                    client::server_error,
                    "engine is shutting down"
                )
            );

            m_queue.pop_front();
        }
    }

    // Signal the slaves to terminate.
    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        unicast(
            specific_slave(*it),
            boost::make_tuple((const int)rpc::terminate)
        );

        it->second->process_event(events::terminate_t());
    }

    m_pool.clear();
    m_tasks.clear();
    m_watcher.stop();
    m_processor.stop();
    m_pumper.stop();
    m_gc_timer.stop();

    return info();
}

Json::Value engine_t::info() const {
    Json::Value results(Json::objectValue);

    if(m_running) {
        results["queue-depth"] = static_cast<Json::UInt>(m_queue.size());
        results["slaves"]["total"] = static_cast<Json::UInt>(m_pool.size());
        
        results["slaves"]["busy"] = static_cast<Json::UInt>(
            std::count_if(
                m_pool.begin(),
                m_pool.end(),
                busy_slave()
            )
        );

        for(task_map_t::const_iterator it = m_tasks.begin(); it != m_tasks.end(); ++it) {
            results["tasks"][it->first] = it->second->info();
        }
    }
    
    results["running"] = m_running;

    return results;
}

// Queue operations
// ----------------

void engine_t::enqueue(job_queue_t::const_reference job, bool overflow) {
    if(!m_running) {
        log().debug(
            "dropping an incomplete '%s' job",
            job->method().c_str()
        );

        job->process_event(
            events::error_t(
                client::server_error,
                "engine is not active"
            )
        );

        return;
    }

    // NOTE: If we got an idle slave, then we're lucky and got an instant scheduling;
    // if not, try to spawn more slaves, and enqueue the job.
    const int command = rpc::invoke;
    
    pool_map_t::iterator it(
        unicast(
            idle_slave(),
            boost::tie(
                command,
                job->method(),
                job->request()
            )
        )
    );

    if(it != m_pool.end()) {
        it->second->process_event(events::invoke_t(job));
    } else {
        if(m_pool.empty() || m_pool.size() < m_app.policy.pool_limit) {
            std::auto_ptr<slaves::slave_t> slave;
            
            try {
                slave.reset(new slaves::generic_t(*this));
                std::string slave_id(slave->id());
                m_pool.insert(slave_id, slave);
            } catch(const std::exception& e) {
                log().error(
                    "unable to spawn more slaves - %s",
                    e.what()
                );
            }
        } else if(!overflow && (m_queue.size() > m_app.policy.queue_limit)) {
            job->process_event(
                events::error_t(
                    client::resource_error,
                    "the queue is full"
                )
            );

            return;
        }
            
        m_queue.push(job);
    }
}

// Slave I/O
// ---------

void engine_t::message(ev::io&, int) {
    if(m_messages.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void engine_t::process(ev::idle&, int) {
    if(m_messages.pending()) {
        std::string slave_id;
        unsigned int command = 0;
        boost::tuple<raw<std::string>, unsigned int&> tier(protect(slave_id), command);
                
        m_messages.recv_multi(tier);
        pool_map_t::iterator slave(m_pool.find(slave_id));

        if(slave != m_pool.end()) {
            const slaves::busy* state =
                slave->second->state_downcast<const slaves::busy*>();
            
            switch(command) {
                case rpc::push: {
                    // TEST: Only active slaves can push the data chunks.
                    BOOST_ASSERT(state != 0 && m_messages.more());

                    zmq::message_t message;
                    m_messages.recv(&message);
                    
                    state->job()->process_event(events::push_t(message));

                    break;
                }
             
                case rpc::error: {
                    unsigned int code = 0;
                    std::string message;
                    boost::tuple<unsigned int&, std::string&> tier(code, message);

                    m_messages.recv_multi(tier);

                    if(state) {
                        state->job()->process_event(
                            events::error_t(
                                static_cast<client::error_code>(code), 
                                message
                            )
                        );
                    } else {
                        log().error("the app seems to be broken - %s", message.c_str());
                        stop();
                        return;
                    }

                    break;
                }

                case rpc::release: {
                    // TEST: Only active slaves can release the job.
                    BOOST_ASSERT(state != 0);
                    slave->second->process_event(events::release_t());
                    break;
                }

                case rpc::terminate: {
                    slave->second->process_event(events::terminate_t());
                    return;
                }
            }

            slave->second->process_event(events::heartbeat_t());

            if(slave->second->state_downcast<const slaves::idle*>() && !m_queue.empty()) {
                // NOTE: This will always succeed due to the test above.
                enqueue(m_queue.front());
                m_queue.pop_front();
            }

            // TEST: Ensure that there're no more message parts pending on the channel.
            BOOST_ASSERT(!m_messages.more());
        } else {
            log().debug(
                "ignoring type %d command from a dead slave %s", 
                command, 
                slave_id.c_str()
            );
            
            m_messages.drop_remaining_parts();
        }
    } else {
        m_processor.stop();
    }
}

void engine_t::pump(ev::timer&, int) {
    message(m_watcher, ev::READ);
}

// Garbage collection
// ------------------

void engine_t::cleanup(ev::timer&, int) {
    typedef std::vector<pool_map_t::key_type> corpse_list_t;
    corpse_list_t corpses;

    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        if(it->second->state_downcast<const slaves::dead*>()) {
            corpses.push_back(it->first);
        }
    }

    if(!corpses.empty()) {
        for(corpse_list_t::iterator it = corpses.begin(); it != corpses.end(); ++it) {
            m_pool.erase(*it);
        }

        log().info(
            "recycled %zu dead %s", 
            corpses.size(),
            corpses.size() == 1 ? "slave" : "slaves"
        );
    }
}

