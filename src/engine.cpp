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
#include "cocaine/rpc.hpp"

using namespace cocaine::engine;
using namespace cocaine::networking;

// Job queue
// ---------

void job_queue_t::push(value_type job) {
    if(job->policy().urgent) {
        push_front(job);
        job->process_event(events::enqueue_t(1));
    } else {
        push_back(job);
        job->process_event(events::enqueue_t(size()));
    }
}

// Basic stuff
// -----------

engine_t::engine_t(context_t& ctx, const std::string& name, const Json::Value& manifest):
    m_context(ctx),
    m_running(false),
    m_app(ctx, name, manifest),
    m_messages(ctx.io(), ZMQ_ROUTER)
#ifdef HAVE_CGROUPS
    , m_cgroup(NULL)
#endif
{
#ifdef HAVE_CGROUPS
    Json::Value limits(manifest["engine"]["resource-limits"]);

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
                    m_app.log->error(
                        "controller '%s' parameter '%s' type is not supported",
                        c->c_str(),
                        p->c_str()
                    );

                    continue;
                }
            }
            
            m_app.log->debug(
                "setting controller '%s' parameter '%s' to %s", 
                c->c_str(),
                p->c_str(),
                boost::lexical_cast<std::string>(cfg[*p]).c_str()
            );
        }
    }

    int rv = 0;

    if((rv = cgroup_create_cgroup(m_cgroup, false)) != 0) {
        m_app.log->error(
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
            m_app.log->error(
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

    m_app.log->info("starting the engine"); 

    int linger = 0;

    m_messages.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    try {
        m_messages.bind(endpoint(m_app.name));
    } catch(const zmq::error_t& e) {
        throw configuration_error_t(std::string("invalid rpc endpoint - ") + e.what());
    }
    
    m_watcher.set<engine_t, &engine_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<engine_t, &engine_t::process>(this);
    m_pumper.set<engine_t, &engine_t::pump>(this);
    m_pumper.start(0.005f, 0.005f);

    m_gc_timer.set<engine_t, &engine_t::cleanup>(this);
    m_gc_timer.start(5.0f, 5.0f);
   
    // Tasks configuration
    // -------------------

    Json::Value tasks(m_app.manifest["tasks"]);

    if(!tasks.isNull() && tasks.size()) {
        Json::Value::Members names(tasks.getMemberNames());

        m_app.log->info(
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
            } else if(type == "server+lsd" || type == "native-server") {
                m_tasks.insert(task, new drivers::native_server_t(*this, task, tasks[task]));
            } else {
               throw configuration_error_t("no driver for '" + type + "' is available");
            }
        }
    } else {
        throw configuration_error_t("no tasks has been specified");
    }

    m_running = true;
}

namespace {
    struct terminate {
        template<class T>
        void operator()(const T& slave) {
            slave->second->process_event(events::terminate_t());
        }
    };
}

void engine_t::stop(std::string status) {
    BOOST_ASSERT(m_running);
    
    m_app.log->info("stopping the engine"); 

    m_running = false;

    if(!status.empty()) {
        m_status = status;
    }

    // Abort all the outstanding jobs.
    if(!m_queue.empty()) {
        m_app.log->debug(
            "dropping %zu queued %s",
            m_queue.size(),
            m_queue.size() == 1 ? "job" : "jobs"
        );

        while(!m_queue.empty()) {
            m_queue.front().process_event(
                events::error_t(
                    dealer::resource_error,
                    "engine is not active"
                )
            );

            m_queue.pop_front();
        }
    }

    rpc::packed<rpc::terminate> packed;

    // Send the termination event to active slaves.
    multicast(select::state<slave::alive>(), packed);

    // XXX: Might be a good idea to wait for a graceful termination.
    std::for_each(m_pool.begin(), m_pool.end(), terminate());

    m_pool.clear();
    m_tasks.clear();

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

        for(task_map_t::iterator it = m_tasks.begin();
            it != m_tasks.end();
            ++it) 
        {
            results["tasks"][it->first] = it->second->info();
        }
    }
    
    results["running"] = m_running;

    if(!m_status.empty()) {
        results["status"] = m_status;
    }

    return results;
}

// Queue operations
// ----------------

void engine_t::enqueue(job_queue_t::value_type job, bool overflow) {
    if(!m_running) {
        m_app.log->debug(
            "dropping an incomplete '%s' job",
            job->method().c_str()
        );

        job->process_event(
            events::error_t(
                dealer::resource_error,
                "engine is not active"
            )
        );

        delete job;
        return;
    }

    events::invoke_t event(job);

    rpc::packed<rpc::invoke> packed(
        job->method(),
        job->request().data(),
        job->request().size()
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
    } else {
        if(m_pool.empty() || 
          (m_pool.size() < m_app.policy.pool_limit && 
           m_pool.size() * m_app.policy.grow_threshold < m_queue.size()))
        {
            std::auto_ptr<slave_t> slave;
            
            try {
                slave.reset(new slave_t(*this));
                std::string slave_id(slave->id());
                m_pool.insert(slave_id, slave);
            } catch(const system_error_t& e) {
                m_app.log->error(
                    "unable to spawn more slaves - %s - %s",
                    e.what(),
                    e.reason()
                );
            }
        }

        if((m_queue.size() >= m_app.policy.queue_limit) && !overflow) {
            job->process_event(
                events::error_t(
                    dealer::resource_error,
                    "the queue is full"
                )
            );

            delete job;
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
    int counter = context().config.defaults.io_bulk_size;
    
    do {
        if(!m_messages.pending()) {
            m_processor.stop();
            return;
        }

        std::string slave_id;
        int command = 0;
        boost::tuple<raw<std::string>, int&> proxy(protect(slave_id), command);
                
        m_messages.recv_multi(proxy);

        pool_map_t::iterator slave(m_pool.find(slave_id));

        if(slave == m_pool.end()) {
            m_app.log->warning(
                "engine dropping type %d event from a nonexistent slave %s", 
                command,
                slave_id.c_str()
            );
            
            m_messages.drop_remaining_parts();
            return;
        }

        switch(command) {
            case rpc::heartbeat:
                slave->second->process_event(events::heartbeat_t());
                break;

            case rpc::push: {
                // TEST: Ensure we have the actual chunk following.
                BOOST_ASSERT(m_messages.more());

                zmq::message_t message;
                m_messages.recv(&message);
                
                slave->second->process_event(events::push_t(message));

                break;
            }
         
            case rpc::invoke: {
                // TEST: Ensure we have the actual delegate following.
                BOOST_ASSERT(m_messages.more());

                std::string target;
                zmq::message_t message;
                boost::tuple<std::string&, zmq::message_t*> proxy(target, &message);

                m_messages.recv_multi(proxy);

                slave->second->process_event(events::delegate_t(target, message));

                break;
            }

            case rpc::error: {
                // TEST: Ensure that we have the actual error following.
                BOOST_ASSERT(m_messages.more());

                int code = 0;
                std::string message;
                boost::tuple<int&, std::string&> proxy(code, message);

                m_messages.recv_multi(proxy);

                slave->second->process_event(events::error_t(code, message));

                if(code == dealer::server_error) {
                    m_app.log->error("the app seems to be broken");
                    stop();
                }

                break;
            }

            case rpc::release:
                slave->second->process_event(events::release_t());
                break;

            default:
                m_app.log->warning("engine dropping unknown event type %d", command);
                m_messages.drop_remaining_parts();
        }

        if(!m_queue.empty() &&
           slave->second->state_downcast<const slave::idle*>())
        {
            while(!m_queue.empty()) {
                job_queue_t::auto_type job(m_queue.release(m_queue.begin()));

                if(!job->state_downcast<const job::complete*>()) {
                    // NOTE: This will always succeed due to the test above.
                    enqueue(job.release());
                    break;
                }
            }
        }

        // TEST: Ensure that there're no more message parts pending on the channel.
        BOOST_ASSERT(!m_messages.more());
    } while(--counter);
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

        m_app.log->debug(
            "recycled %zu dead %s", 
            corpses.size(),
            corpses.size() == 1 ? "slave" : "slaves"
        );
    }
}
