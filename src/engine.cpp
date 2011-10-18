#include <iomanip>
#include <sstream>

#include <boost/assign.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "cocaine/drivers.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/security/digest.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::helpers;
using namespace cocaine::lines;
using namespace cocaine::plugin;
using namespace cocaine::security;

/*
Json::Value core_t::past(const Json::Value& args) {
    std::string key(args["key"].asString());

    if(key.empty()) {
        throw std::runtime_error("no driver id has been specified");
    }

    history_map_t::iterator it(m_histories.find(key));

    if(it == m_histories.end()) {
        throw std::runtime_error("the past for a given key is empty");
    }

    uint32_t depth = args.get("depth", config_t::get().core.history_depth).asUInt(),
             counter = 0;
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

engine_t::engine_t(zmq::context_t& context, const std::string& uri):
    m_context(context),
    m_uri(uri),
    m_config(config_t::get().engine),
    m_messages(m_context, ZMQ_ROUTER)
{
    syslog(LOG_INFO, "engine %s [%s]: starting", id().c_str(), m_uri.c_str());
    
    m_messages.bind("inproc://engines/" + id());
    
    m_message_watcher.set<engine_t, &engine_t::message>(this);
    m_message_watcher.start(m_messages.fd(), EV_READ);
    m_message_processor.set<engine_t, &engine_t::process_message>(this);
    m_message_processor.start();
}

engine_t::~engine_t() {
    syslog(LOG_INFO, "engine %s [%s]: terminating", id().c_str(), m_uri.c_str()); 
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

    std::string server(manifest["server:endpoint"].asString()),
                pubsub(manifest["pubsub:endpoint"].asString());
    Json::Value tasks(manifest["tasks"]), result(Json::objectValue);

    m_config.heartbeat_timeout = manifest.get("server:heartbeat-timeout",
        config_t::get().engine.heartbeat_timeout).asUInt();
    m_config.queue_depth = manifest.get("server:queue-depth",
        config_t::get().engine.queue_depth).asUInt();
    m_config.worker_limit = manifest.get("server:worker-limit",
        config_t::get().engine.worker_limit).asUInt();
    m_config.history_depth = manifest.get("pubsub:history-depth",
        config_t::get().engine.history_depth).asUInt();

    if(!server.empty()) {
        m_application = manifest["server:application"].asString();

        if(m_application.empty()) {
            throw std::runtime_error("no application has been specified for serving");
        }

        std::string route = config_t::get().core.route + "/" +
                            digest_t().get(m_uri);
        result["server:route"] = route;
        
        m_server.reset(new socket_t(m_context, ZMQ_ROUTER, route));
        m_server->bind(server);

        m_request_watcher.reset(new ev::io());
        m_request_watcher->set<engine_t, &engine_t::request>(this);
        m_request_watcher->start(m_server->fd(), ev::READ);
        m_request_processor.reset(new ev::idle());
        m_request_processor->set<engine_t, &engine_t::process_request>(this);
        m_request_processor->start();
    }

    if(!tasks.isNull() && tasks.size()) {
        if(!pubsub.empty()) {
            m_pubsub.reset(new socket_t(m_context, ZMQ_PUB));
            m_pubsub->bind(pubsub);
        }

        Json::Value::Members names = tasks.getMemberNames();

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            std::string type(tasks[*it]["type"].asString());
            
            if(types.find(type) == types.end()) {
               throw std::runtime_error("invalid task type - '" + type + "'");
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

    result["engine:status"] = "running";

    return result;
}

template<class DriverType>
void engine_t::schedule(const std::string& task, const Json::Value& args) {
    std::auto_ptr<DriverType> driver(new DriverType(task, shared_from_this(), args));
    std::string driver_id(driver->id());

    if(m_tasks.find(driver_id) == m_tasks.end()) {
        driver->start();
        m_tasks.insert(driver_id, driver);
    } else {
        throw std::runtime_error("duplicate task");
    }
}

void engine_t::stop() {
    if(m_server) {
        m_request_watcher->stop();
        m_request_processor->stop();
    }
    
    m_tasks.clear();
    
    for(thread_map_t::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
        m_messages.send_multi(boost::make_tuple(
            protect(it->first),
            TERMINATE));
        m_threads.erase(it);
    }
    
    m_message_watcher.stop();
    m_message_processor.stop();
}

namespace {
    struct nonempty_queue {
        bool operator()(engine_t::thread_map_t::reference thread) {
            return thread->second->queue_size() != 0;
        }
    };
}

Json::Value engine_t::stats() {
    Json::Value results;

    results["route"] = config_t::get().core.route + "/" + digest_t().get(m_uri);
    
    results["threads:total"] = static_cast<Json::UInt>(m_threads.size());
    results["threads:active"] = static_cast<Json::UInt>(std::count_if(
            m_threads.begin(),
            m_threads.end(),
            nonempty_queue()));
    results["threads:limit"] = m_config.worker_limit;
    
    thread_map_t::const_iterator it(std::max_element(
            m_threads.begin(), 
            m_threads.end(), 
            shortest_queue()));

    results["queues:deepest"] = it != m_threads.end() ?
        static_cast<Json::UInt>(it->second->queue_size()) : 
        0;
    results["queues:limit"] = m_config.queue_depth;

    return results;
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
    if(m_pubsub) {
        zmq::message_t message;
        ev::tstamp now = ev::get_default_loop().now();

        // Maintain the history for the given driver
        history_map_t::iterator history(m_histories.find(key));

        if(history == m_histories.end()) {
            boost::tie(history, boost::tuples::ignore) = m_histories.insert(key,
                new history_t());
        } else if(history->second->size() == m_config.history_depth) {
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
        std::string thread_id;
        unsigned int code = 0;

        boost::tuple<raw<std::string>, unsigned int&> tier(protect(thread_id), code);
        m_messages.recv_multi(tier);

        thread_map_t::iterator thread(m_threads.find(thread_id));

        if(thread != m_threads.end()) {
            switch(code) {
                case FUTURE: {
                    boost::shared_ptr<future_t> future(thread->second->queue_pop());
                    Json::Value object;
                            
                    m_messages.recv(object);
                    future->push(object);

                    break;
                }

                case HEARTBEAT: {
                    thread->second->rearm(m_config.heartbeat_timeout);
                    break;
                }

                case SUICIDE: {
                    m_threads.erase(thread);
                    break;
                }

                default:
                    syslog(LOG_CRIT, "engine %s [%s]: [%s()] unknown message",
                        id().c_str(), m_uri.c_str(), __func__);
                    abort();
            }
        } else {
            syslog(LOG_WARNING, "engine %s [%s]: [%s()] dropping messages for thread %s", 
                id().c_str(), m_uri.c_str(), __func__, thread_id.c_str());
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
                    m_application,
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

// Thread interface
// ----------------

thread_t::thread_t(unique_id_t::type engine_id, zmq::context_t& context, const std::string& uri):
    m_engine_id(engine_id),
    m_context(context),
    m_uri(uri)
{
    syslog(LOG_DEBUG, "thread %s [%s]: starting", id().c_str(), m_engine_id.c_str());
   
    m_heartbeat.set<thread_t, &thread_t::timeout>(this);
    
    // Creates an overseer and attaches it to a thread 
    create();
}

thread_t::~thread_t() {
    if(m_thread) {
        syslog(LOG_DEBUG, "thread %s [%s]: terminating", id().c_str(), m_engine_id.c_str());

        m_heartbeat.stop();

#if BOOST_VERSION >= 103500
        if(!m_thread->timed_join(boost::posix_time::seconds(5))) {
            syslog(LOG_WARNING, "thread %s [%s]: thread is unresponsive",
                id().c_str(), m_engine_id.c_str());
            m_thread->interrupt();
        }
#else
        m_thread->join();
#endif
    }
}

void thread_t::create() {
    try {
        // Init the overseer and all the sockets
        m_overseer.reset(new overseer_t(id(), m_engine_id, m_context));

        // Create a new source
        boost::shared_ptr<source_t> source(core::registry_t::instance()->create(m_uri));
        
        // Launch the thread
        m_thread.reset(new boost::thread(boost::bind(
            &overseer_t::run, m_overseer.get(), source)));

        // First heartbeat is only to ensure that the thread has started
        rearm(10.);
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit exceeded");
    }
}

void thread_t::timeout(ev::timer& w, int revents) {
    syslog(LOG_ERR, "thread %s [%s]: thread missed a heartbeat", id().c_str(), m_engine_id.c_str());

#if BOOST_VERSION >= 103500
    m_thread->interrupt();
#endif

    // Send timeouts to all the pending requests
    Json::Value object(Json::objectValue);
    object["error"] = "timed out";

    while(!m_queue.empty()) {
        m_queue.front()->push(object);
        m_queue.pop();
    }

    // XXX: This doesn't work yet, as ZeroMQ doesn't support two sockets with the same identity
    // XXX: i.e., when the old thread has been interrupted, and a new one spawns with the same id
    // XXX: the new socket should overtake the older identity, but it crashes instead
    create();
}

void thread_t::rearm(float timeout) {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }

    m_heartbeat.start(timeout);
}
