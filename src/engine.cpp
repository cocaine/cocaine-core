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
    m_channel(m_context, ZMQ_ROUTER)
{
    syslog(LOG_INFO, "engine %s [%s]: starting", id().c_str(), m_uri.c_str());
    
    m_channel.bind("inproc://engine/" + id());
    m_channel_watcher.set<engine_t, &engine_t::event>(this);
    m_channel_watcher.start(m_channel.fd(), EV_READ);

    // Have to bootstrap it
    ev::get_default_loop().feed_fd_event(m_channel.fd(), ev::READ);
}

engine_t::~engine_t() {
    syslog(LOG_INFO, "engine %s [%s]: terminating", id().c_str(), m_uri.c_str()); 
}

// Operations
// ----------

Json::Value engine_t::run(const Json::Value& manifest) {
    static std::map<const std::string, unsigned int> types = boost::assign::map_list_of
        ("auto", AUTO)
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
        std::string route = config_t::get().core.hostname + "/" + 
                            config_t::get().core.instance + "/" +
                            digest_t().get(m_uri);
        
        m_server.reset(new socket_t(m_context, ZMQ_ROUTER, route));
        m_server->bind(server);

        m_server_watcher.reset(new ev::io());
        m_server_watcher->set<engine_t, &engine_t::request>(this);
        m_server_watcher->start(m_server->fd(), ev::READ);
    
        result["server:route"] = route;
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

void engine_t::stop() {
    if(m_server) {
        m_server_watcher->stop();
    }
    
    m_tasks.clear();
    
    for(thread_map_t::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
        m_channel.send_multi(boost::make_tuple(
            protect(it->first),
            TERMINATE));
        m_threads.erase(it);
    }
    
    m_channel_watcher.stop();
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

// Publishing format (not JSON, as it will render subscription mechanics pointless):
// ------------------
//   multipart: [key field hostname timestamp] [blob]

void engine_t::publish(const std::string& task, const Json::Value& event) {
    zmq::message_t message;
    ev::tstamp now = ev::get_default_loop().now();

    // Maintain the history for the given driver
    history_map_t::iterator history(m_histories.find(task));

    if(history == m_histories.end()) {
        boost::tie(history, boost::tuples::ignore) = m_histories.insert(task,
            new history_t());
    } else if(history->second->size() == m_config.history_depth) {
        history->second->pop_back();
    }
    
    history->second->push_front(std::make_pair(now, event));

    // Disassemble and send in the envelopes
    Json::Value::Members members(event.getMemberNames());

    for(Json::Value::Members::iterator it = members.begin(); it != members.end(); ++it) {
        std::string key(*it);
        
        std::ostringstream envelope;
        envelope << task << " " << key << " " << config_t::get().core.hostname << " "
                 << std::fixed << std::setprecision(3) << now;

        message.rebuild(envelope.str().length());
        memcpy(message.data(), envelope.str().data(), envelope.str().length());
        m_pubsub->send(message, ZMQ_SNDMORE);

        Json::Value object(event[key]);
        std::string value;

        switch(object.type()) {
            case Json::booleanValue:
                value = object.asBool() ? "true" : "false";
                break;
            case Json::intValue:
            case Json::uintValue:
                value = boost::lexical_cast<std::string>(object.asInt());
                break;
            case Json::realValue:
                value = boost::lexical_cast<std::string>(object.asDouble());
                break;
            case Json::stringValue:
                value = object.asString();
                break;
            default:
                value = "<error: unable to publish non-primitive types>";
        }

        message.rebuild(value.length());
        memcpy(message.data(), value.data(), value.length());
        m_pubsub->send(message);
    }
}

void engine_t::event(ev::io& w, int revents) {
    std::string thread_id;
    unsigned int code = 0;

    while((revents & ev::READ) && m_channel.pending()) {
        boost::tuple<raw<std::string>, unsigned int&> tier(protect(thread_id), code);
        m_channel.recv_multi(tier);

        thread_map_t::iterator thread(m_threads.find(thread_id));

        if(thread != m_threads.end()) {
            switch(code) {
                case FUTURE: {
                    boost::shared_ptr<future_t> future(thread->second->queue_pop());
                    Json::Value object;
                            
                    m_channel.recv(object);
                    future->push(object);
                    
                    break;
                }

                case EVENT: {
                    std::string task;
                    Json::Value object;

                    boost::tuple<std::string&, Json::Value&> tier(task, object);
                    m_channel.recv_multi(tier);
        
                    if(m_pubsub) {
                        publish(task, object);
                    }

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
                    syslog(LOG_ERR, "engine %s [%s]: [%s()] unknown message",
                        id().c_str(), m_uri.c_str(), __func__);
                    abort();
            }
        } else {
            syslog(LOG_DEBUG, "engine %s [%s]: [%s()] outstanding messages - thread %s", 
                id().c_str(), m_uri.c_str(), __func__, thread_id.c_str());
            m_channel.ignore();
        }
    }
}

void engine_t::request(ev::io& w, int revents) {
    zmq::message_t message;
    std::vector<std::string> route;

    while((revents & ev::READ) && m_server->pending()) {
        route.clear();
        
        while(true) {
            m_server->recv(&message);

            if(!message.size()) {
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }
       
        boost::shared_ptr<response_t> response(
            new response_t(route, m_server));
        
        m_server->recv(&message);

        try {
            response->wait(queue(
                boost::make_tuple(
                    PROCESS,
                    std::string("Serve"),
                    std::string(
                        static_cast<const char*>(message.data()),
                        message.size())
                    )
                )
            );
        } catch(const std::runtime_error& e) {
            response->abort(e.what());
        }
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
        throw std::runtime_error("system thread limit reached");
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

    create();
}

void thread_t::rearm(float timeout) {
    if(m_heartbeat.is_active()) {
        m_heartbeat.stop();
    }

    m_heartbeat.start(timeout);
}
