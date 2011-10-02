#include <boost/bind.hpp>

#include "cocaine/threading.hpp"
#include "cocaine/drivers.hpp"
#include "cocaine/future.hpp" 
#include "cocaine/storage.hpp"

using namespace cocaine::engine::threading;
using namespace cocaine::engine::drivers;

using namespace cocaine::plugin;
using namespace cocaine::storage;
using namespace cocaine::helpers;

overseer_t::overseer_t(auto_uuid_t id, zmq::context_t& context):
    m_id(id),
    m_context(context),
    m_upstream(m_context, ZMQ_PULL),
    m_downstream(m_context, ZMQ_PUSH),
    m_loop(),
    m_request(m_loop),
    m_suicide(m_loop)
//  m_cleanup(m_loop),
//  m_cached(false)
{
    // Connect to the engine's controlling socket
    // and set the socket watcher
    m_upstream.connect("inproc://" + m_id.get());
    m_request.set<overseer_t, &overseer_t::request>(this);
    m_request.start(m_upstream.fd(), EV_READ);

    // Initializing suicide timer
    m_suicide.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide.start(config_t::get().engine.suicide_timeout);
    
    // Cache cleanup watcher
    // m_cleanup.set<overseer_t, &overseer_t::cleanup>(this);
    // m_cleanup.start();

    // Connecting to the core's downstream channel
    m_downstream.connect("inproc://core");

    // Set timer compression threshold
    m_loop.set_timeout_collect_interval(config_t::get().engine.collect_timeout);

    // Signal a false event, in case the core 
    // has managed to send something already
    m_loop.feed_fd_event(m_upstream.fd(), EV_READ);
}

void overseer_t::run(boost::shared_ptr<source_t> source) {
    m_source = source;
    m_loop.loop();
}

void overseer_t::request(ev::io& w, int revents) {
    unsigned int code = 0;

    while(m_upstream.pending()) {
        // Get the message code
        m_upstream.recv(code);

        switch(code) {
            case PUSH:
            case DROP: {
                std::string future_id, driver_type;
                Json::Value args, result;

                // Get the remaining payload
                boost::tuple<std::string&, Json::Value&> tier(future_id, args);
                m_upstream.recv_multi(tier);

                try {
                    std::string driver_type(args.get("driver", "auto").asString());

                    if(driver_type == "auto") {
                        result = dispatch<drivers::auto_t>(code, args);
                    } else if(driver_type == "manual") {
                        result = dispatch<drivers::manual_t>(code, args);
                    } else if(driver_type == "fs") {
                        result = dispatch<drivers::fs_t>(code, args);
                    } else if(driver_type == "zeromq") {
                        result = dispatch<drivers::zeromq_t>(code, args);
                    } else if(driver_type == "once") {
                        result = once(args);
                    } else {
                        throw std::runtime_error("invalid driver type");
                    }
                } catch(const std::runtime_error& e) {
                    result["error"] = e.what();
                }

                m_downstream.send_multi(boost::make_tuple(
                    FUTURE,
                    future_id,
                    m_source->uri(),
                    result));

                break;
            }
            case TERMINATE:
                terminate();
                return;
        }
    }
}
 
void overseer_t::timeout(ev::timer& w, int revents) {
    suicide();
}

/* XXX: I wonder if this caching is needed at all
void overseer_t::cleanup(ev::prepare& w, int revents) {
    m_cache.clear();
    m_cached = false;
}

const Json::Value& overseer_t::invoke() {
    if(!m_cached) {
        try {
            m_cache = m_source->invoke();
            m_cached = true;
        } catch(const std::exception& e) {
            syslog(LOG_ERR, "engine %s: error - %s", m_source->uri().c_str(), e.what());
            m_cache["error"] = e.what();
        }
    }

    return m_cache;
}
*/

template<class DriverType>
Json::Value overseer_t::dispatch(unsigned int code, const Json::Value& args) {
    switch(code) {
        case PUSH:
            return push<DriverType>(args);
        case DROP:
            return drop<DriverType>(args);
    }
}

template<class DriverType>
Json::Value overseer_t::push(const Json::Value& args) {
    Json::Value result;
    std::auto_ptr<DriverType> driver(new DriverType(this, m_source, args));
    std::string driver_id(driver->id());
    
    // Scheduling
    if(m_slaves.find(driver_id) == m_slaves.end()) {
        driver->start();
        m_slaves.insert(driver_id, driver);

        if(m_suicide.is_active()) {
            syslog(LOG_DEBUG, "engine %s: suicide timer stopped", m_source->uri().c_str());
            m_suicide.stop();
        }
    }
   
    result["key"] = driver_id;

    // ACL
    subscription_map_t::const_iterator begin, end;
    boost::tie(begin, end) = m_subscriptions.equal_range(driver_id);
    
    std::string token(args["token"].asString());
    subscription_map_t::value_type subscription(std::make_pair(driver_id, token));
    
    std::equal_to<subscription_map_t::value_type> equality;

    if(std::find_if(begin, end, boost::bind(equality, subscription, _1)) == end) {
        syslog(LOG_DEBUG, "engine %s: subscribing '%s'", m_source->uri().c_str(),
            token.c_str());
        m_subscriptions.insert(subscription);
    }
    
    // Persistance
    if(!args.get("transient", false).asBool()) {
        std::string thread_id(args.get("isolated", false).asBool() ? m_id.get() : "");
        std::string object_id(m_digest.get(driver_id + token + thread_id));
       
        try { 
            if(!storage_t::instance()->exists("tasks", object_id)) {
                Json::Value object(Json::objectValue);
                
                object["uri"] = m_source->uri();
                object["args"] = args;
                
                if(!thread_id.empty()) {
                    object["args"]["thread"] = thread_id;
                    result["thread"] = thread_id;
                }

                storage_t::instance()->put("tasks", object_id, object);
            }
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "engine %s: storage failure while pushing a task - %s",
                m_source->uri().c_str(), e.what());
        }
    }

    return result;
}

template<class DriverType>
Json::Value overseer_t::drop(const Json::Value& args) {
    std::auto_ptr<DriverType> driver(new DriverType(this, m_source, args));
    slave_map_t::iterator slave;
    subscription_map_t::iterator client;
   
    // Check if the driver is active 
    if((slave = m_slaves.find(driver->id())) != m_slaves.end()) {
        subscription_map_t::iterator begin, end;
        boost::tie(begin, end) = m_subscriptions.equal_range(driver->id());
        
        std::string token(args["token"].asString());
        subscription_map_t::value_type subscription(std::make_pair(driver->id(), token));
        
        std::equal_to<subscription_map_t::value_type> equality;

        // Check if the client is a subscriber
        if((client = std::find_if(begin, end, boost::bind(equality, subscription, _1))) != end) {
            // If it is the last subscription, stop the driver
            if(std::distance(begin, end) == 1) {
                m_slaves.erase(slave);
            }
           
            // Unsubscribe
            m_subscriptions.erase(client);

            // If it was the last slave, start the suicide timer
            if(m_slaves.empty()) {
                if(!args.get("isolated", false).asBool()) {
                    syslog(LOG_DEBUG, "engine %s: suicide timer started",
                        m_source->uri().c_str());
                    m_suicide.start(config_t::get().engine.suicide_timeout);
                } else {
                    // Or, if it was an isolated thread, suicide.
                    suicide();
                }
            }

            // Un-persist
            std::string thread_id(args.get("isolated", false).asBool() ? m_id.get() : "");
            std::string object_id(m_digest.get(driver->id() + token + thread_id));
            
            try {
                storage_t::instance()->remove("tasks", object_id);
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "engine %s: storage failure while dropping a task - %s",
                    m_source->uri().c_str(), e.what());
            }
        } else {
            throw std::runtime_error("access denied");
        }
    } else {
        throw std::runtime_error("driver is not active");
    }

    Json::Value result;
    result["status"] = "success";

    return result;
}

Json::Value overseer_t::once(const Json::Value& args) {
    Json::Value result;
    
    try {
        result = m_source->invoke();
    } catch(const std::exception& e) {
        result["error"] = e.what();
    }

    if(!args.get("isolated", false).asBool()) {
        // Rearm the stall timer if it's active
        if(m_suicide.is_active()) {
            syslog(LOG_DEBUG, "engine %s: suicide timer rearmed", m_source->uri().c_str());
            m_suicide.stop();
            m_suicide.start(config_t::get().engine.suicide_timeout);
        }
    } else {
        // If it's a one-time isolated task, then it was a kamikaze mission,
        // and we can safely kill ourselves
        suicide();
    }

    return result;
}

void overseer_t::reap(const std::string& driver_id) {
    slave_map_t::iterator it(m_slaves.find(driver_id));

    if(it == m_slaves.end()) {
        syslog(LOG_ERR, "engine %s: found an orphan - driver %s", 
            m_source->uri().c_str(), driver_id.c_str());
        return;
    }

    m_slaves.erase(it);

    // XXX: Have to suicide here, if it was an isolated thread
    if(m_slaves.empty()) {
        syslog(LOG_DEBUG, "engine %s: suicide timer started", m_source->uri().c_str());
        m_suicide.start(config_t::get().engine.suicide_timeout);
    }
}

void overseer_t::terminate() {
    m_slaves.clear();
    m_source.reset();
    m_loop.unloop();
} 

void overseer_t::suicide() {
    m_downstream.send_multi(boost::make_tuple(
        SUICIDE,
        m_source->uri(), /* engine id */
        m_id.get()));    /* thread id */
}

// Thread interface
// ----------------

thread_t::thread_t(auto_uuid_t id, zmq::context_t& context):
    m_id(id),
    m_downstream(context, ZMQ_PUSH)
{
    // Bind the messaging channel
    m_downstream.bind("inproc://" + m_id.get());
 
    // Initialize the overseer
    m_overseer.reset(new overseer_t(id, context));
}

thread_t::~thread_t() {
    if(m_thread.get()) {
        syslog(LOG_DEBUG, "thread %s: terminating", m_id.get().c_str());
   
        m_downstream.send(TERMINATE);

#if BOOST_VERSION >= 103500
        using namespace boost::posix_time;
        
        if(!m_thread->timed_join(seconds(config_t::get().engine.linger_timeout))) {
            syslog(LOG_WARNING, "thread %s: thread is unresponsive", m_id.get().c_str());
            m_thread->interrupt();
        }
#else
        m_thread->join();
#endif
    }
}

void thread_t::run(boost::shared_ptr<source_t> source) {
    syslog(LOG_DEBUG, "thread %s: starting for %s", m_id.get().c_str(),
        source->uri().c_str());

    try {
        m_thread.reset(new boost::thread(
            boost::bind(&overseer_t::run, m_overseer.get(), source)));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit reached");
    }
}

void thread_t::request(unsigned int code, core::future_t* future, const Json::Value& args) {
    m_downstream.send_multi(boost::make_tuple(
        code,
        future->id(),
        args));
}

void thread_t::track() {

}
