#include <iomanip>
#include <sstream>

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>
#include <boost/lexical_cast.hpp>

#include "cocaine/drivers.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/registry.hpp"

using namespace cocaine::engine;
using namespace cocaine::lines;

bool engine_t::idle_worker::operator()(pool_map_t::reference worker) {
    return worker.second->state() == idle;
}

engine_t::engine_t(zmq::context_t& context, const std::string& name):
    m_running(false),
    m_context(context),
    m_messages(m_context, ZMQ_ROUTER)
{
    m_app_cfg.name = name;

    syslog(LOG_DEBUG, "engine [%s]: constructing", m_app_cfg.name.c_str());
   
    m_messages.bind(boost::algorithm::join(
        boost::assign::list_of
            (std::string("ipc:///var/run/cocaine"))
            (config_t::get().core.instance)
            (m_app_cfg.name),
        "/"));
    
    m_watcher.set<engine_t, &engine_t::message>(this);
    m_watcher.start(m_messages.fd(), ev::READ);
    m_processor.set<engine_t, &engine_t::process>(this);
    m_processor.start();
}

engine_t::~engine_t() {
    syslog(LOG_DEBUG, "engine [%s]: destructing", m_app_cfg.name.c_str()); 
}

// Operations
// ----------

Json::Value engine_t::start(const Json::Value& manifest) {
    // Application configuration
    // -------------------------

    m_app_cfg.type = manifest["type"].asString();
    m_app_cfg.args = manifest["args"].asString();

    if(!core::registry_t::instance()->exists(m_app_cfg.type)) {
        throw std::runtime_error("no plugin for '" + m_app_cfg.type + "' is available");
    }
    
    syslog(LOG_INFO, "engine [%s]: starting", m_app_cfg.name.c_str()); 
    
    // Pool configuration
    // ------------------

    m_policy.backend = manifest["engine"].get("backend",
        config_t::get().engine.backend).asString();
    
    if(m_policy.backend != "thread" && m_policy.backend != "process") {
        throw std::runtime_error("invalid backend type");
    }
    
    m_policy.suicide_timeout = manifest["engine"].get("suicide-timeout",
        config_t::get().engine.suicide_timeout).asDouble();
    m_policy.pool_limit = manifest["engine"].get("pool-limit",
        config_t::get().engine.pool_limit).asUInt();
    m_policy.queue_limit = manifest["engine"].get("queue-limit",
        config_t::get().engine.queue_limit).asUInt();
    
    // Tasks configuration
    // -------------------

    Json::Value tasks(manifest["tasks"]);

    if(!tasks.isNull() && tasks.size()) {
        std::string endpoint(manifest["pubsub"]["endpoint"].asString());
        
        if(!endpoint.empty()) {
            m_pubsub.reset(new socket_t(m_context, ZMQ_PUB));
            m_pubsub->bind(endpoint);
        }

        Json::Value::Members names(tasks.getMemberNames());

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            std::string task(*it);
            std::string type(tasks[task]["type"].asString());
            
            if(type == "timed+auto") {
                schedule<drivers::auto_timed_t>(task, tasks[task]);
            } else if(type == "fs") {
                schedule<drivers::fs_t>(task, tasks[task]);
            } else if(type == "server+zmq") {
                schedule<drivers::zmq_server_t>(task, tasks[task]);
            } else if(type == "server+lsd") {
                schedule<drivers::lsd_server_t>(task, tasks[task]);
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

template<class DriverType>
void engine_t::schedule(const std::string& method, const Json::Value& args) {
    m_tasks.insert(method, new DriverType(this, method, args));
}

Json::Value engine_t::stop() {
    syslog(LOG_INFO, "engine [%s]: stopping", m_app_cfg.name.c_str()); 
    
    m_running = false;

    // Abort all the outstanding jobs 
    while(!m_queue.empty()) {
        m_queue.front()->send(server_error, "engine is shutting down");
        m_queue.pop_front();
    }

    // Stop all the tasks to cut the event flow
    for(task_map_t::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        it->second->stop();
    }

    // Signal the workers to terminate
    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        m_messages.send_multi(
            boost::make_tuple(
                protect(it->first),
                TERMINATE));
        it->second->stop();
    }

    // Wait for the pool to terminate
    while(!m_pool.empty()) {
        ev::get_default_loop().loop(ev::ONESHOT);
    }

    m_tasks.clear();
    m_watcher.stop();
    m_processor.stop();

    return info();
}

namespace {
    struct active_worker {
        bool operator()(engine_t::pool_map_t::const_reference worker) {
            return worker.second->state() == active;
        }
    };
}

Json::Value engine_t::info() const {
    Json::Value results(Json::objectValue);

    results["queue"] = static_cast<Json::UInt>(m_queue.size());
    
    results["pool"]["total"] = static_cast<Json::UInt>(m_pool.size());
    
    results["pool"]["active"] = static_cast<Json::UInt>(
        std::count_if(
            m_pool.begin(),
            m_pool.end(),
            active_worker()
        ));

    for(task_map_t::const_iterator it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        results["tasks"][it->first] = it->second->info();
    }
    
    results["running"] = m_running;
    
    return results;
}

void engine_t::reap(unique_id_t::reference worker_id) {
    pool_map_t::iterator worker(m_pool.find(worker_id));

    if(worker->second->state() == active) {
        pool_map_t::auto_type corpse(m_pool.release(worker));
        
        if(m_running) { 
            syslog(LOG_DEBUG, "engine [%s]: requeueing a job from a dead worker",
                m_app_cfg.name.c_str());
            corpse->job()->enqueue();
        } else {
            corpse->job()->send(server_error, "engine is shutting down");
            corpse->job().reset();
        }
    } else {
        m_pool.erase(worker);
    }
}

void engine_t::expire(boost::shared_ptr<job_t> job) {
    syslog(LOG_DEBUG, "engine [%s]: dropping an expired job", m_app_cfg.name.c_str());
   
    job->send(timeout_error, "the job has expired");
    m_queue.erase(std::find(m_queue.begin(), m_queue.end(), job));
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
            envelope << key << " " << field << " " << config_t::get().core.hostname << " "
                     << std::fixed << std::setprecision(3) << now;

            message.rebuild(envelope.str().length());
            memcpy(message.data(), envelope.str().data(), envelope.str().length());
            m_pubsub->send(message, ZMQ_SNDMORE);

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

            message.rebuild(result.length());
            memcpy(message.data(), result.data(), result.length());
            m_pubsub->send(message);
        }
    }
}

// Worker I/O
// ----------

void engine_t::message(ev::io&, int) {
    if(m_messages.pending()) {
        m_watcher.stop();
        m_processor.start();
    }
}

void engine_t::process(ev::idle&, int) {
    if(m_messages.pending()) {
        std::string worker_id;
        unsigned int code = 0;

        boost::tuple<raw<std::string>, unsigned int&> tier(protect(worker_id), code);
        m_messages.recv_multi(tier);

        pool_map_t::iterator worker(m_pool.find(worker_id));

        if(worker != m_pool.end()) {
            // NOTE: Any type of message is suitable to rearm the heartbeat timer
            // so we don't have to do anything special about HEARBEAT messages at all,
            // it's a dummy message to send in the periods of inactivity.
            worker->second->rearm();
           
            switch(code) {
                case CHUNK: {
                    zmq::message_t chunk;

                    m_messages.recv(&chunk);

                    worker->second->job()->send(chunk);

                    return;
                }
             
                case ERROR: {
                    std::string message;

                    m_messages.recv(message);
                    
                    worker->second->job()->send(application_error, message);

                    return;
                }

                case CHOKE: {
                    ev::tstamp spent = 0;

                    m_messages.recv(spent);

                    worker->second->job()->audit(spent);
                    worker->second->job().reset();
                    worker->second->rearm();
                   
                    break;
                }

                case SUICIDE:
                    worker->second->stop();
                    
                    return;

                case TERMINATE:
                    syslog(LOG_ERR, "engine [%s]: the application seems to be broken",
                        m_app_cfg.name.c_str());
                    
                    stop();
                    
                    return;
            }

            if(worker->second->state() == idle && !m_queue.empty()) {
                boost::shared_ptr<job_t> job(m_queue.front());
                m_queue.pop_front();

                // NOTE: This will always succeed due to the test above
                job->enqueue();
            }
        } else {
            syslog(LOG_WARNING, "engine [%s]: dropping type %d messages from a dead worker %s", 
                m_app_cfg.name.c_str(), code, worker_id.c_str());
            m_messages.ignore();
        }
    } else {
        m_watcher.start(m_messages.fd(), ev::READ);
        m_processor.stop();
    }
}

