#include <iomanip>
#include <sstream>

#include <boost/assign.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include "cocaine/drivers.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine;
using namespace cocaine::lines;

engine_t::engine_t(zmq::context_t& context, const std::string& name):
    m_context(context),
    m_messages(m_context, ZMQ_ROUTER)
{
    m_app_cfg.name = name;

    syslog(LOG_DEBUG, "engine [%s]: constructing", m_app_cfg.name.c_str());
    
    m_messages.bind("inproc://engines/" + m_app_cfg.name);
    
    m_message_watcher.set<engine_t, &engine_t::message>(this);
    m_message_watcher.start(m_messages.fd(), ev::READ);
    m_message_processor.set<engine_t, &engine_t::process_message>(this);
    m_message_processor.start();
}

engine_t::~engine_t() {
    syslog(LOG_DEBUG, "engine [%s]: destructing", m_app_cfg.name.c_str()); 
}

// Operations
// ----------

Json::Value engine_t::run(const Json::Value& manifest) {
    static std::map<const std::string, unsigned int> types = boost::assign::map_list_of
        ("auto", AUTO)
    //  ("cron", CRON)
    //  ("manual", MANUAL)
        ("fs", FILESYSTEM)
        ("sink", SINK);

    m_app_cfg.type = manifest["type"].asString();
    m_app_cfg.args = manifest["args"].asString();

    if(!core::registry_t::instance()->exists(m_app_cfg.type)) {
        throw std::runtime_error("no plugin for '" + m_app_cfg.type + "' is available");
    }
    
    syslog(LOG_INFO, "engine [%s]: starting", m_app_cfg.name.c_str()); 
    
    Json::Value tasks(manifest["tasks"]), result(Json::objectValue);
    
    m_app_cfg.server_endpoint = manifest["server"]["endpoint"].asString(),
    m_app_cfg.pubsub_endpoint = manifest["pubsub"]["endpoint"].asString();

    m_pool_cfg.heartbeat_timeout = manifest["engine"].get("heartbeat-timeout",
        config_t::get().engine.heartbeat_timeout).asDouble();
    m_pool_cfg.suicide_timeout = manifest["engine"].get("suicide-timeout",
        config_t::get().engine.suicide_timeout).asDouble();
    m_pool_cfg.history_limit = manifest["engine"].get("history-limit",
        config_t::get().engine.history_limit).asUInt();
    m_pool_cfg.pool_limit = manifest["engine"].get("pool-limit",
        config_t::get().engine.pool_limit).asUInt();
    m_pool_cfg.queue_limit = manifest["engine"].get("queue-limit",
        config_t::get().engine.queue_limit).asUInt();

    if(!m_app_cfg.server_endpoint.empty()) {
        m_app_cfg.callable = manifest["server"]["callable"].asString();

        if(m_app_cfg.callable.empty()) {
            throw std::runtime_error("no callable has been specified for serving");
        }

        m_app_cfg.route = config_t::get().core.route + "/" + m_app_cfg.name;

        result["server"]["route"] = m_app_cfg.route;
        
        m_server.reset(new socket_t(m_context, ZMQ_ROUTER, m_app_cfg.route));
        m_server->bind(m_app_cfg.server_endpoint);

        m_request_watcher.reset(new ev::io());
        m_request_watcher->set<engine_t, &engine_t::request>(this);
        m_request_watcher->start(m_server->fd(), ev::READ);
        m_request_processor.reset(new ev::idle());
        m_request_processor->set<engine_t, &engine_t::process_request>(this);
    }

    if(!tasks.isNull() && tasks.size()) {
        if(!m_app_cfg.pubsub_endpoint.empty()) {
            m_pubsub.reset(new socket_t(m_context, ZMQ_PUB));
            m_pubsub->bind(m_app_cfg.pubsub_endpoint);
        }

        Json::Value::Members names(tasks.getMemberNames());

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            std::string type(tasks[*it]["type"].asString());
            
            if(types.find(type) == types.end()) {
               throw std::runtime_error("no scheduler for '" + type + "' is available");
            }

            switch(types[type]) {
                case AUTO:
                    schedule<drivers::auto_t>(*it, tasks[*it]);
                    break;
            //  case CRON:
            //      schedule<drivers::cron_t>(*it, tasks[*it]);
            //      break;
            //  case MANUAL:
            //      schedule<drivers::maual_t>(*it, tasks[*it]);
            //      break;
                case FILESYSTEM:
                    schedule<drivers::fs_t>(*it, tasks[*it]);
                    break;
                case SINK:
                    schedule<drivers::sink_t>(*it, tasks[*it]);
                    break;
            }
        }
    }

    result["engine"]["status"] = "running";

    return result;
}

template<class DriverType>
void engine_t::schedule(const std::string& task, const Json::Value& args) {
    std::auto_ptr<DriverType> driver(new DriverType(task, shared_from_this(), args));
    std::string driver_id(driver->id());
    task_map_t::iterator it(m_tasks.find(driver_id));

    if(it == m_tasks.end()) {
        driver->start();
        m_tasks.insert(driver_id, driver);
    } else {
        boost::format message("task '%1%' is a duplicate of '%2%'");
        throw std::runtime_error((message % task % it->second->name()).str());
    }
}

Json::Value engine_t::stop() {
    Json::Value result;

    syslog(LOG_INFO, "engine [%s]: stopping", m_app_cfg.name.c_str()); 
    
    if(m_server) {
        m_request_watcher->stop();
        m_request_processor->stop();
    }
    
    m_tasks.clear();
    
    for(pool_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        m_messages.send_multi(boost::make_tuple(
            protect(it->first),
            unique_id_t().id(),
            TERMINATE));
        m_pool.erase(it);
    }
    
    m_message_watcher.stop();
    m_message_processor.stop();

    result["engine"]["status"] = "stopped";
    
    return result;
}

namespace {
    struct nonempty_queue {
        bool operator()(engine_t::pool_t::reference worker) {
            return worker->second->queue().size() != 0;
        }
    };
}

Json::Value engine_t::stats() {
    Json::Value results;

    if(m_server) {
        results["server"]["route"] = m_app_cfg.route;
        results["server"]["endpoint"] = m_app_cfg.server_endpoint;
    }

    results["pool"]["total"] = static_cast<Json::UInt>(m_pool.size());
    results["pool"]["active"] = static_cast<Json::UInt>(std::count_if(
        m_pool.begin(),
        m_pool.end(),
        nonempty_queue()));
    results["pool"]["limit"] = m_pool_cfg.pool_limit;

    for(pool_t::const_iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        results["pool"]["queues"][it->first] = 
            static_cast<Json::UInt>(it->second->queue().size());
    }

    if(!m_tasks.empty()) {
        for(task_map_t::const_iterator it = m_tasks.begin(); it != m_tasks.end(); ++it) {
            results["tasks"]["active"].append(it->second->name());
        }
        
        if(m_pubsub) {
            results["tasks"]["pubsub"] = m_app_cfg.pubsub_endpoint;
        }
    }

    return results;
}

/*
Json::Value engine_t::past(const std::string& task) {
    history_map_t::iterator it(m_histories.find(task));

    if(it == m_histories.end()) {
        throw std::runtime_error("the history for a given task is empty");
    }

    Json::Value result(Json::arrayValue);

    for(history_t::const_iterator event = it->second->begin(); event != it->second->end(); ++event) {
        Json::Value object(Json::objectValue);

        object["timestamp"] = event->first;
        object["event"] = event->second;

        result.append(object);

        if(++counter == depth) {
            break;
        }
    }

    return result;
}
*/

void engine_t::reap(unique_id_t::reference worker_id) {
    pool_t::iterator worker(m_pool.find(worker_id));

    // TODO: Re-assign tasks
    if(worker != m_pool.end()) {
        Json::Value object(Json::objectValue);

        object["error"] = "timeout";

        for(worker_type::request_queue_t::iterator it = worker->second->queue().begin(); 
            it != worker->second->queue().end();
            ++it) 
        {
            it->second->push(object);
        }
        
        m_pool.erase(worker);
    }
}

// Future support
// --------------

void engine_t::respond(const route_t& route, const Json::Value& object) {
    if(m_server) {
        zmq::message_t message;
        
        // Send the identity
        for(lines::route_t::const_iterator id = route.begin(); id != route.end(); ++id) {
            message.rebuild(id->length());
            memcpy(message.data(), id->data(), id->length());
#if ZMQ_VERSION < 30000
            m_server->send(message, ZMQ_SNDMORE);
#else
            m_server->send(message, ZMQ_SNDMORE | ZMQ_SNDLABEL);
#endif
        }

#if ZMQ_VERSION < 30000                
        // Send the delimiter
        message.rebuild(0);
        m_server->send(message, ZMQ_SNDMORE);
#endif

        Json::FastWriter writer;
        std::string json(writer.write(object));
        message.rebuild(json.length());
        memcpy(message.data(), json.data(), json.length());

        m_server->send(message);
    }
}

void engine_t::publish(const std::string& key, const Json::Value& object) {
    if(m_pubsub && object.isObject()) {
        zmq::message_t message;
        ev::tstamp now = ev::get_default_loop().now();

        // Maintain the history for the given driver
        history_map_t::iterator history(m_histories.find(key));

        if(history == m_histories.end()) {
            boost::tie(history, boost::tuples::ignore) = m_histories.insert(key,
                new history_t());
        } else if(history->second->size() == m_pool_cfg.history_limit) {
            history->second->pop_back();
        }
        
        history->second->push_front(std::make_pair(now, object));

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

// Thread I/O
// ----------

void engine_t::message(ev::io& w, int revents) {
    if(m_messages.pending() && !m_message_processor.is_active()) {
        m_message_processor.start();
    }
}

void engine_t::process_message(ev::idle& w, int revents) {
    if(m_messages.pending()) {
        std::string worker_id;
        unsigned int code = 0;

        boost::tuple<raw<std::string>, unsigned int&> tier(protect(worker_id), code);
        m_messages.recv_multi(tier);

        pool_t::iterator worker(m_pool.find(worker_id));

        if(worker != m_pool.end()) {
            switch(code) {
                case FULFILL: {
                    unique_id_t::type promise_id;
                    Json::Value object;

                    boost::tuple<std::string&, Json::Value&> tier(promise_id, object);
                    m_messages.recv_multi(tier);

                    worker_type::request_queue_t::iterator request(
                        worker->second->queue().find(promise_id));
                           
                    if(request != worker->second->queue().end()) {
                        request->second->push(object);
                        worker->second->queue().erase(request);
                    } else {
                        syslog(LOG_ERR, "engine [%s]: received a response from a wrong worker",
                            m_app_cfg.name.c_str());
                    }
                    
                    break;
                }
                
                case HEARTBEAT:
                    worker->second->rearm(m_pool_cfg.heartbeat_timeout);
                    break;

                case SUICIDE:
                    m_pool.erase(worker);
                    break;

                default:
                    syslog(LOG_DEBUG, "engine [%s]: trash on channel", m_app_cfg.name.c_str());
            }
        } else {
            syslog(LOG_ERR, "engine [%s]: dropping messages for orphaned worker %s", 
                m_app_cfg.name.c_str(), worker_id.c_str());
            m_messages.ignore();
        }
    } else {
        m_message_processor.stop();
    }
}

// Application I/O
// ---------------

void engine_t::request(ev::io& w, int revents) {
    if(m_server->pending() && !m_request_processor->is_active()) {
        m_request_processor->start();
    }
}

void engine_t::process_request(ev::idle& w, int revents) {
    if(m_server->pending()) {
        zmq::message_t message;
        std::vector<std::string> route;

        while(true) {
            m_server->recv(&message);

#if ZMQ_VERSION > 30000
            if(!m_server->is_label()) {
#else
            if(!message.size()) {
#endif
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }
      
#if ZMQ_VERSION < 30000
        m_server->recv(&message);
#endif

        boost::shared_ptr<response_t> response(
            new response_t(route, shared_from_this()));
        
        try {
            response->wait(queue(
                boost::make_tuple(
                    INVOKE,
                    m_app_cfg.callable,
                    std::string(
                        static_cast<const char*>(message.data()),
                        message.size())
                    )
                )
            );
        } catch(const std::runtime_error& e) {
            response->abort(e.what());
        }
    } else {
        m_request_processor->stop();
    }
}

