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

#include <iomanip>
#include <sstream>

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include "cocaine/context.hpp"
#include "cocaine/drivers.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/rpc.hpp"

using namespace cocaine::engine;
using namespace cocaine::networking;

void engine_t::job_queue_t::push(const_reference job) {
    if(job->policy().urgent) {
        push_front(job);
        job->process_event(events::enqueued_t(1));
    } else {
        push_back(job);
        job->process_event(events::enqueued_t(size()));
    }
}

// Selectors
// ---------

bool engine_t::idle_slave::operator()(pool_map_t::pointer slave) const {
    return slave->second->state_downcast<const slave::idle*>();
}

// Basic stuff
// -----------

engine_t::engine_t(context_t& context, const std::string& name):
    identifiable_t((boost::format("engine [%s]") % name).str()),
    m_context(context),
    m_running(false),
    m_messages(*m_context.bus, ZMQ_ROUTER)
{
    int linger = 0;
    m_app_cfg.name = name;

    syslog(LOG_DEBUG, "%s: constructing", identity());

    m_messages.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
    m_messages.bind(boost::algorithm::join(
        boost::assign::list_of
            (std::string("ipc:///var/run/cocaine"))
            (m_context.config.core.instance)
            (m_app_cfg.name),
        "/")
    );
    
    m_watcher.set<engine_t, &engine_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<engine_t, &engine_t::process>(this);
    m_pumper.set<engine_t, &engine_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);

    m_gc_timer.set<engine_t, &engine_t::cleanup>(this);
    m_gc_timer.start(5.0f, 5.0f);
}

engine_t::~engine_t() {
    if(m_running) {
        stop();
    }

    syslog(LOG_DEBUG, "%s: destructing", identity()); 
}

// Operations
// ----------

Json::Value engine_t::start(const Json::Value& manifest) {
    BOOST_ASSERT(!m_running);

    // Application configuration
    // -------------------------

    m_app_cfg.type = manifest["type"].asString();
    m_app_cfg.args = manifest["args"].asString();
    m_app_cfg.version = manifest.get("version", 1).asUInt();

    if(!core::registry_t::instance(m_context)->exists(m_app_cfg.type)) {
        throw std::runtime_error("no plugin for '" + m_app_cfg.type + "' is available");
    }
    
    // Pool configuration
    // ------------------

    m_policy.backend = manifest["engine"].get("backend",
        m_context.config.engine.backend).asString();
    
    if(m_policy.backend != "thread" && m_policy.backend != "process") {
        throw std::runtime_error("invalid backend type");
    }
    
#if BOOST_VERSION < 103500
    if(m_policy.backend == "thread") {
        syslog(LOG_WARNING, "%s: system doesn't support unresponsive thread termination", identity());
    }
#endif

    m_policy.suicide_timeout = manifest["engine"].get("suicide-timeout",
        m_context.config.engine.suicide_timeout).asDouble();
    m_policy.pool_limit = manifest["engine"].get("pool-limit",
        m_context.config.engine.pool_limit).asUInt();
    m_policy.queue_limit = manifest["engine"].get("queue-limit",
        m_context.config.engine.queue_limit).asUInt();
    
    // Tasks configuration
    // -------------------

    Json::Value tasks(manifest["tasks"]);

    if(!tasks.isNull() && tasks.size()) {
        syslog(LOG_INFO, "%s: starting", identity()); 
    
        std::string endpoint(manifest["pubsub"]["endpoint"].asString());
        
        if(!endpoint.empty()) {
            m_pubsub.reset(new socket_t(*m_context.bus, ZMQ_PUB));
            m_pubsub->bind(endpoint);
        }

        Json::Value::Members names(tasks.getMemberNames());

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            std::string task(*it);
            std::string type(tasks[task]["type"].asString());
            
            if(type == "recurring-timer" || type == "timed+auto") {
                m_tasks.insert(task, new driver::recurring_timer_t(*this, task, tasks[task]));
            } else if(type == "drifting-timer") {
                m_tasks.insert(task, new driver::drifting_timer_t(*this, task, tasks[task]));
            } else if(type == "filesystem-monitor") {
                m_tasks.insert(task, new driver::filesystem_monitor_t(*this, task, tasks[task]));
            } else if(type == "zeromq-server") {
                m_tasks.insert(task, new driver::zeromq_server_t(*this, task, tasks[task]));
            } else if(type == "server+lsd") {
                m_tasks.insert(task, new driver::lsd_server_t(*this, task, tasks[task]));
            } else if(type == "native-server") {
                m_tasks.insert(task, new driver::native_server_t(*this, task, tasks[task]));
            } else if(type == "zeromq-sink") {
                m_tasks.insert(task, new driver::zeromq_sink_t(*this, task, tasks[task]));
            } else if(type == "native-sink") {
                m_tasks.insert(task, new driver::native_sink_t(*this, task, tasks[task]));
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
    
    syslog(LOG_INFO, "%s: stopping", identity()); 

    m_running = false;

    // Abort all the outstanding jobs 
    syslog(LOG_DEBUG, "%s: dropping %zu queued %s", identity(), m_queue.size(), 
        m_queue.size() == 1 ? "job" : "jobs");

    while(!m_queue.empty()) {
        m_queue.front()->process_event(
            events::error_t(
                client::server_error, 
                "engine is shutting down"
            )
        );

        m_queue.pop_front();
    }

    // Signal the slaves to terminate
    rpc::terminate_t terminator;
    
    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        // NOTE: Doesn't matter if it's not delivered, slaves will be killed anyway.
        m_messages.send_multi(
            boost::tie(
                protect(it->first),
                terminator.type,
                terminator
            )
        );
        
        it->second->process_event(events::terminated_t());
    }

    m_pool.clear();
    m_tasks.clear();
    m_watcher.stop();
    m_processor.stop();
    m_pumper.stop();
    m_gc_timer.stop();

    return info();
}

namespace {
    struct busy_slave {
        bool operator()(engine_t::pool_map_t::const_pointer slave) {
            return slave->second->state_downcast<const slave::busy*>();
        }
    };
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
    results["version"] = m_app_cfg.version;

    return results;
}

void engine_t::enqueue(job_queue_t::const_reference job, bool overflow) {
    if(!m_running) {
        job->process_event(
            events::error_t(
                client::server_error,
                "engine is not active"
            )
        );

        return;
    }

    pool_map_t::iterator it(
        unicast(
            idle_slave(),
            rpc::invoke_t(job->driver().method()),
            job->request()
        )
    );

    // NOTE: If we got an idle slave, then we're lucky and got an instant scheduling;
    // if not, try to spawn more slaves, and enqueue the job.
    if(it != m_pool.end()) {
        it->second->process_event(events::invoked_t(job));
    } else {
        if(m_pool.empty() || m_pool.size() < m_policy.pool_limit) {
            std::auto_ptr<slave::slave_t> slave;
            
            try {
                if(m_policy.backend == "thread") {
                    slave.reset(new slave::thread_t(*this, m_app_cfg.type, m_app_cfg.args));
                } else if(m_policy.backend == "process") {
                    slave.reset(new slave::process_t(*this, m_app_cfg.type, m_app_cfg.args));
                }
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "%s: unable to spawn more workers - %s", identity(), e.what());
            }

            std::string slave_id(slave->id());
            m_pool.insert(slave_id, slave);
        } else if(!overflow && (m_queue.size() > m_policy.queue_limit)) {
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

// PubSub Interface
// ----------------

void engine_t::publish(const std::string& key, const Json::Value& object) {
    if(m_pubsub && object.isObject()) {
        zmq::message_t message;
        ev::tstamp now = ev::get_default_loop().now();

        // Disassemble and send in the envelopes
        Json::Value::Members members(object.getMemberNames());

        for(Json::Value::Members::iterator it = members.begin(); it != members.end(); ++it) {
            std::string field(*it);
            
            std::ostringstream envelope;
            envelope << key << " " << field << " " << m_context.config.core.hostname << " "
                     << std::fixed << std::setprecision(3) << now;

            message.rebuild(envelope.str().size());
            memcpy(message.data(), envelope.str().data(), envelope.str().size());
            
            if(m_pubsub->send(message, ZMQ_SNDMORE)) {
                Json::Value value(object[field]);
                std::string result;

                switch(value.type()) {
                    case Json::booleanValue:
                        result = value.asBool() ? "true" : "false";
                        break;
                    case Json::intValue:
                    case Json::uintValue:
                        result = boost::lexical_cast<std::string>(value.asInt());
                        break;
                    case Json::realValue:
                        result = boost::lexical_cast<std::string>(value.asDouble());
                        break;
                    case Json::stringValue:
                        result = value.asString();
                        break;
                    default:
                        result = boost::lexical_cast<std::string>(value);
                }

                message.rebuild(result.size());
                memcpy(message.data(), result.data(), result.size());
                
                if(!m_pubsub->send(message)) {
                    syslog(LOG_ERR, "%s: unable to publish the object", identity());
                }
            } else {
                syslog(LOG_ERR, "%s: unable to publish the object", identity());
            }
        }
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
        unsigned int code = 0;
        boost::tuple<raw<std::string>, unsigned int&> tier(protect(slave_id), code);
        
        BOOST_VERIFY(m_messages.recv_multi(tier));
        
        pool_map_t::iterator slave(m_pool.find(slave_id));

        if(slave != m_pool.end()) {
            const slave::busy* state =
                slave->second->state_downcast<const slave::busy*>();
            
            switch(code) {
                case rpc::heartbeat: {
                    rpc::heartbeat_t object;

                    BOOST_VERIFY(m_messages.recv(object));
                    BOOST_ASSERT(object.type == rpc::heartbeat);

                    break;
                }

                case rpc::chunk: {
                    rpc::chunk_t object;
                    zmq::message_t message;

                    boost::tuple<rpc::chunk_t&, zmq::message_t*> tier(object, &message);
                    
                    BOOST_VERIFY(m_messages.recv_multi(tier));
                    BOOST_ASSERT(object.type == rpc::chunk);
                    BOOST_ASSERT(state != 0);

                    state->job()->process_event(events::chunk_t(message));

                    break;
                }
             
                case rpc::error: {
                    rpc::error_t object;

                    BOOST_VERIFY(m_messages.recv(object));
                    BOOST_ASSERT(object.type == rpc::error);
                    BOOST_ASSERT(state || object.code == client::server_error);

                    if(state) {
                        state->job()->process_event(
                            events::error_t(
                                static_cast<client::error_code>(object.code),
                                object.message
                            )
                        );
                    } else {
                        syslog(LOG_ERR, "%s: [%d] %s", identity(), object.code, 
                            object.message.c_str());
                        publish("engine", helpers::make_json("error", object.message));
                    }
                    
                    if(object.code == client::server_error) {
                        syslog(LOG_ERR, "%s: the application seems to be broken", identity());
                        stop();
                        return;
                    }

                    break;
                }

                case rpc::choke: {
                    rpc::choke_t object;

                    BOOST_VERIFY(m_messages.recv(object));
                    BOOST_ASSERT(object.type == rpc::choke);
                    BOOST_ASSERT(state != 0);
                   
                    slave->second->process_event(events::choked_t());
                    
                    break;
                }

                case rpc::terminate: {
                    rpc::terminate_t object;

                    BOOST_VERIFY(m_messages.recv(object));
                    BOOST_ASSERT(object.type == rpc::terminate);

                    // NOTE: A slave might be already terminated by its inner mechanics
                    if(!slave->second->state_downcast<const slave::dead*>()) {
                        slave->second->process_event(events::terminated_t());
                    }

                    return;
                }
            }

            slave->second->process_event(events::heartbeat_t());

            if(slave->second->state_downcast<const slave::idle*>() && !m_queue.empty()) {
                // NOTE: This will always succeed due to the test above
                enqueue(m_queue.front());
                m_queue.pop_front();
            }

            // TEST: Ensure that there're no more message parts pending on the channel
            BOOST_ASSERT(!m_messages.more());
        } else {
            syslog(LOG_DEBUG, "%s: dropping type %d message from a dead slave %s", 
                identity(), code, slave_id.c_str());
            m_messages.drop_remaining_parts();
        }
    } else {
        m_processor.stop();
    }
}

void engine_t::pump(ev::timer&, int) {
    message(m_watcher, ev::READ);
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
        for(corpse_list_t::iterator it = corpses.begin(); it != corpses.end(); ++it) {
            m_pool.erase(*it);
        }

        syslog(LOG_INFO, "%s: recycled %zu dead %s", identity(), corpses.size(),
            corpses.size() == 1 ? "slave" : "slaves");
    }
}

publication_t::publication_t(driver::driver_t& parent, const client::policy_t& policy):
    job::job_t(parent, policy)
{ }

void publication_t::react(const events::chunk_t& event) {
    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(reader.parse(
        static_cast<const char*>(event.message.data()),
        static_cast<const char*>(event.message.data()) + event.message.size(),
        root))
    {
        m_driver.engine().publish(m_driver.method(), root);
    } else {
        m_driver.engine().publish(m_driver.method(), 
            helpers::make_json(
                "error", 
                "unable to parse the response json"
            )
        );
    }
}

