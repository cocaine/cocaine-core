#include <boost/bind.hpp>

#include "cocaine/threading.hpp"
#include "cocaine/drivers.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::engine::threading;
using namespace cocaine::engine::drivers;

using namespace cocaine::plugin;
using namespace cocaine::storage;
using namespace cocaine::helpers;

overseer_t::overseer_t(auto_uuid_t id, zmq::context_t& context):
    m_id(id),
    m_context(context),
    m_pipe(m_context, ZMQ_PULL),
    m_futures(m_context, ZMQ_PUSH),
    m_reaper(m_context, ZMQ_PUSH),
    m_loop(),
    m_io(m_loop),
    m_suicide(m_loop),
    m_cleanup(m_loop),
    m_cached(false)
{
    // Connect to the engine's controlling socket
    // and set the socket watcher
    m_pipe.connect("inproc://" + m_id.get());
    m_io.set<overseer_t, &overseer_t::request>(this);
    m_io.start(m_pipe.fd(), EV_READ);

    // Initializing suicide timer
    m_suicide.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide.start(config_t::get().engine.suicide_timeout);
    
    // Cache cleanup watcher
    m_cleanup.set<overseer_t, &overseer_t::cleanup>(this);
    m_cleanup.start();

    // Connecting to the core's future sink
    m_futures.connect("inproc://futures");

    // Connecting to the core's reaper sink
    m_reaper.connect("inproc://reaper");

    // Set timer compression threshold
    m_loop.set_timeout_collect_interval(config_t::get().engine.collect_timeout);

    // Signal a false event, in case the core 
    // has managed to send something already
    m_loop.feed_fd_event(m_pipe.fd(), EV_READ);
}

void overseer_t::run(boost::shared_ptr<source_t> source) {
    m_source = source;
    m_loop.loop();
}

void overseer_t::request(ev::io& w, int revents) {
    Json::Value result;
    std::string command, driver;
    
    while(m_pipe.pending()) {
        Json::Value message;
        
        m_pipe.recv_json(message);

        command = message["command"].asString();
        driver = message["args"].get("driver", "auto").asString(); 
       
        if(command == "start") {
            if(driver == "auto") {
                push<drivers::auto_t>(message);
            } else if(driver == "manual") {
                push<drivers::manual_t>(message);
            } else if(driver == "fs") {
                push<drivers::fs_t>(message);
            } else if(driver == "event") {
                push<drivers::event_t>(message);
            } else if(driver == "once") {
                once(message);
            } else {
                result["error"] = "invalid driver";
                respond(message["future"], result);
            }
        } else if(command == "stop") {
            if(driver == "auto") {
                drop<drivers::auto_t>(message);
            } else if(driver == "manual") {
                drop<drivers::manual_t>(message);
            } else if(driver == "fs") {
                drop<drivers::fs_t>(message);
            } else if(driver == "event") {
                drop<drivers::event_t>(message);
            } else {
                result["error"] = "invalid driver";
                respond(message["future"], result);
            }
        } else if(command == "terminate") {
            terminate();
            break;
        } else {
            result["error"] = "invalid command";
            respond(message["future"], result);
        }
    }
}
 
void overseer_t::timeout(ev::timer& w, int revents) {
    suicide();
}

void overseer_t::cleanup(ev::prepare& w, int revents) {
    m_cache.clear();
    m_cached = false;
}

const dict_t& overseer_t::invoke() {
    if(!m_cached) {
        try {
            m_cache = m_source->invoke();
            m_cached = true;
        } catch(const std::exception& e) {
            syslog(LOG_ERR, "engine %s: error - %s", m_source->uri().c_str(), e.what());
            suicide();
        }
    }

    return m_cache;
}

template<class DriverType>
void overseer_t::push(const Json::Value& message) {
    Json::Value result;
    std::string token = message["future"]["token"].asString();
    std::string compartment;

    if(message["args"].get("isolated", false).asBool()) {
        compartment = m_id.get();
    };

    std::auto_ptr<DriverType> driver;
    std::string driver_id;

    try {
        driver.reset(new DriverType(m_source, message["args"]));
        driver_id = driver->id();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "engine %s: error - %s", m_source->uri().c_str(), e.what());
        result["error"] = e.what();
        respond(message["future"], result);
        return;
    }
    
    // Scheduling
    if(m_slaves.find(driver_id) == m_slaves.end()) {
        try {
            driver->start(m_context, this);
        } catch(const std::exception& e) {
            syslog(LOG_ERR, "engine %s: error - %s", m_source->uri().c_str(), e.what());
            result["error"] = e.what();
            respond(message["future"], result);
            return;
        }
            
        m_slaves.insert(driver_id, driver);

        if(m_suicide.is_active()) {
            syslog(LOG_DEBUG, "engine %s: suicide timer stopped", m_source->uri().c_str());
            m_suicide.stop();
        }
    }

    // ACL
    subscription_map_t::const_iterator begin, end;
    boost::tie(begin, end) = m_subscriptions.equal_range(token);
    subscription_map_t::value_type subscription = std::make_pair(token, driver_id);
    std::equal_to<subscription_map_t::value_type> equality;

    if(std::find_if(begin, end, boost::bind(equality, subscription, _1)) == end) {
        syslog(LOG_DEBUG, "engine %s: subscribing '%s'", m_source->uri().c_str(),
            token.c_str());
        m_subscriptions.insert(subscription);
    }
    
    // Persistance
    if(!message["args"].get("transient", false).asBool()) {
        std::string object_id = m_digest.get(driver_id + token + compartment);
       
        try { 
            if(!storage_t::instance()->exists("tasks", object_id)) {
                Json::Value object;
                
                object["url"] = m_source->uri();
                object["args"] = message["args"];
                object["token"] = message["future"]["token"];
                
                if(!compartment.empty()) {
                    object["args"]["compartment"] = compartment;
                }

                storage_t::instance()->put("tasks", object_id, object);
            }
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "engine %s: storage failure - %s",
                m_source->uri().c_str(), e.what());
        }
    }

    // Report to the core
    result["key"] = driver_id;

    if(!compartment.empty()) {
        result["compartment"] = compartment;
    }

    respond(message["future"], result);
}

template<class DriverType>
void overseer_t::drop(const Json::Value& message) {
    Json::Value result;
    std::string token = message["future"]["token"].asString();
    std::string compartment;

    if(message["args"].get("isolated", false).asBool()) {
        compartment = m_id.get();
    };

    std::auto_ptr<DriverType> driver;
    std::string driver_id;

    try {
        driver.reset(new DriverType(m_source, message["args"]));
        driver_id = driver->id();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "engine %s: error - %s", m_source->uri().c_str(), e.what());
        result["error"] = e.what();
        respond(message["future"], result);
        return;
    }
   
    slave_map_t::iterator slave;
    subscription_map_t::iterator client;

    if((slave = m_slaves.find(driver_id)) != m_slaves.end()) {
        subscription_map_t::iterator begin, end;
        boost::tie(begin, end) = m_subscriptions.equal_range(token);
        subscription_map_t::value_type subscription = std::make_pair(token, driver_id);
        std::equal_to<subscription_map_t::value_type> equality;

        if((client = std::find_if(begin, end, boost::bind(equality, subscription, _1))) != end) {
            m_slaves.erase(slave);
            m_subscriptions.erase(client);
           
            if(m_slaves.empty()) {
                if(message["args"].get("isolated", false).asBool()) {
                    suicide();
                } else {
                    syslog(LOG_DEBUG, "engine %s: suicide timer started", m_source->uri().c_str());
                    m_suicide.start(config_t::get().engine.suicide_timeout);
                }
            }

            // Un-persist
            std::string object_id = m_digest.get(driver_id + token + compartment);
            
            try {
                storage_t::instance()->remove("tasks", object_id);
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "engine %s: storage failure - %s",
                    m_source->uri().c_str(), e.what());
            }

            result = "success";
        } else {
            result["error"] = "not authorized";
        }
    } else {
        result["error"] = "driver not found";
    }

    respond(message["future"], result);
}

void overseer_t::once(const Json::Value& message) {
    Json::Value result;
    const dict_t& dict = invoke();

    for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
        result[it->first] = it->second;
    }

    // Report to the core
    respond(message["future"], result);

    // If it's a one-time isolated task, then it was a kamikaze mission,
    // and we can safely kill ourselves
    if(message["args"].get("isolated", false).asBool()) {
        suicide();
        return;
    }

    // Rearm the stall timer if it's active
    if(m_suicide.is_active()) {
        syslog(LOG_DEBUG, "engine %s: suicide timer rearmed", m_source->uri().c_str());
        m_suicide.stop();
        m_suicide.start(config_t::get().engine.suicide_timeout);
    }
}

void overseer_t::reap(const std::string& driver_id) {
    slave_map_t::iterator it = m_slaves.find(driver_id);

    if(it == m_slaves.end()) {
        syslog(LOG_ERR, "engine %s: found an orphan - driver %s", 
            m_source->uri().c_str(), driver_id.c_str());
        return;
    }

    m_slaves.erase(it);

    if(m_slaves.empty()) {
        syslog(LOG_DEBUG, "engine %s: suicide timer started", m_source->uri().c_str());
        m_suicide.start(config_t::get().engine.suicide_timeout);
    }
}

void overseer_t::terminate() {
    m_io.stop();
    m_suicide.stop();
    m_cleanup.stop();
    m_slaves.clear();
    m_loop.unloop();
} 

void overseer_t::suicide() {
    Json::Value message;

    message["engine"] = m_source->uri();
    message["thread"] = m_id.get();

    // This is a suicide ;(
    m_reaper.send_json(message);    
}

thread_t::thread_t(auto_uuid_t id, zmq::context_t& context):
    m_id(id),
    m_pipe(context, ZMQ_PUSH)
{
    // Bind the messaging pipe
    m_pipe.bind("inproc://" + m_id.get());
 
    // Initialize the overseer
    m_overseer.reset(new overseer_t(id, context));
}

thread_t::~thread_t() {
    Json::Value message;
    
    syslog(LOG_DEBUG, "thread %s: terminating", m_id.get().c_str());
   
    if(m_thread.get()) { 
        message["command"] = "terminate";
        send(message);

        m_thread->join();
    }
}

void thread_t::run(boost::shared_ptr<source_t> source) {
    syslog(LOG_DEBUG, "thread %s: starting for %s", m_id.get().c_str(),
        source->uri().c_str());

    try {
        m_thread.reset(new boost::thread(boost::bind(&overseer_t::run, m_overseer.get(), source)));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("thread limit reached");
    }
}

