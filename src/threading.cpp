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
{
    // Connect to the engine's controlling socket and set the socket watcher
    m_upstream.connect("inproc://" + m_id.get());
    m_request.set<overseer_t, &overseer_t::request>(this);
    m_request.start(m_upstream.fd(), EV_READ);

    // Initializing suicide timer
    m_suicide.set<overseer_t, &overseer_t::timeout>(this);
    m_suicide.start(config_t::get().engine.suicide_timeout);
    
    // Connecting to the core's downstream channel
    m_downstream.connect("inproc://core");

    // Set timer compression threshold
    m_loop.set_timeout_collect_interval(config_t::get().engine.collect_timeout);

    // Signal a false event, in case the core has managed to send something already
    m_loop.feed_fd_event(m_upstream.fd(), EV_READ);
}

void overseer_t::run(boost::shared_ptr<source_t> source) {
    m_source = source;
    m_loop.loop();
}

void overseer_t::request(ev::io& w, int revents) {
    unsigned int code = 0;

    while(m_upstream.pending()) {
        std::string future_id, target_id;
        Json::Value result(Json::objectValue);
        
        // Get the message code
        m_upstream.recv(code);

        switch(code) {
            case PUSH: {
                std::string driver_type;
                Json::Value args;

                // Get the remaining payload
                boost::tuple<std::string&, std::string&, Json::Value&>
                    tier(future_id, target_id, args);
                m_upstream.recv_multi(tier);

                try {
                    driver_type = args.get("driver", "auto").asString();

                    if(driver_type == "auto") {
                        result = push<drivers::auto_t>(args);
                    } else if(driver_type == "manual") {
                        result = push<drivers::manual_t>(args);
                    } else if(driver_type == "fs") {
                        result = push<drivers::fs_t>(args);
                    } else if(driver_type == "sink") {
                        result = push<drivers::sink_t>(args);
                    } else if(driver_type == "server") {
                        result = push<drivers::server_t>(args);
                    } else if(driver_type == "once") {
                        result = once();
                    } else {
                        throw std::runtime_error("invalid driver type");
                    }
                } catch(const std::runtime_error& e) {
                    syslog(LOG_ERR, "thread %s in %s: [%s()] %s",
                        m_id.get().c_str(), m_source->uri().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                break;
            }
            case DROP: {
                std::string driver_id;

                // Get the remaining payload
                boost::tuple<std::string&, std::string&, std::string&>
                    tier(future_id, target_id, driver_id);
                m_upstream.recv_multi(tier);

                try {
                    result = drop(driver_id);
                } catch(const std::runtime_error& e) {
                    syslog(LOG_ERR, "thread %s in %s: [%s()] %s",
                        m_id.get().c_str(), m_source->uri().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                break;
            }
            case TERMINATE:
                terminate();
                return;
        }
       
        // Report to the core 
        result["thread"] = m_id.get();
        
        m_downstream.send_multi(boost::make_tuple(
            FUTURE,
            future_id,
            target_id,
            result));
    }
}
 
void overseer_t::timeout(ev::timer& w, int revents) {
    m_downstream.send_multi(boost::make_tuple(
        SUICIDE,
        m_source->uri(), /* engine id */
        m_id.get()));    /* thread id */
}

template<class DriverType>
Json::Value overseer_t::push(const Json::Value& args) {
    Json::Value result(Json::objectValue);

    std::auto_ptr<DriverType> driver(new DriverType(this, m_source, args));
    std::string driver_id(driver->id());
    
    // Scheduling
    if(m_slaves.find(driver_id) == m_slaves.end()) {
        driver->start();
        m_slaves.insert(driver_id, driver);

        if(m_suicide.is_active()) {
            syslog(LOG_DEBUG, "thread %s in %s: suicide timer stopped",
                m_id.get().c_str(), m_source->uri().c_str());
            m_suicide.stop();
        }
    }
   
    result["key"] = driver_id;

    // Persistance
    if(!args.get("transient", false).asBool()) {
        std::string object_id(m_digest.get(m_id.get() + driver_id));
       
        try { 
            if(!storage_t::instance()->exists("tasks", object_id)) {
                Json::Value object(args);
                
                object["thread"] = m_id.get();

                storage_t::instance()->put("tasks", object_id, object);
            }
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "thread %s in %s: [%s()] storage failure - %s",
                m_id.get().c_str(), m_source->uri().c_str(), __func__, e.what());
        }
    }

    return result;
}

Json::Value overseer_t::drop(const std::string& driver_id) {
    slave_map_t::iterator slave;
   
    // Check if the driver is active 
    if((slave = m_slaves.find(driver_id)) != m_slaves.end()) {
        m_slaves.erase(slave);
       
        // If it was the last slave, start the suicide timer
        if(m_slaves.empty()) {
            syslog(LOG_DEBUG, "thread %s in %s: suicide timer started",
                m_id.get().c_str(), m_source->uri().c_str());
            m_suicide.start(config_t::get().engine.suicide_timeout);
        }

        // Un-persist
        std::string object_id(m_digest.get(m_id.get() + driver_id));
        
        try {
            storage_t::instance()->remove("tasks", object_id);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "thread %s in %s: [%s()] storage failure - %s",
                m_id.get().c_str(), m_source->uri().c_str(), __func__, e.what());
        }
    } else {
        throw std::runtime_error("driver is not active");
    }

    Json::Value result(Json::objectValue);
    result["status"] = "success";

    return result;
}

Json::Value overseer_t::once() {
    Json::Value result(Json::objectValue);
    
    try {
        result["result"] = m_source->invoke();
    } catch(const std::exception& e) {
        syslog(LOG_ERR, "thread %s in %s: [%s()] %s",
            m_id.get().c_str(), m_source->uri().c_str(), __func__, e.what());
        result["error"] = e.what();
    }

    // Rearm the stall timer if it's active
    if(m_suicide.is_active()) {
        syslog(LOG_DEBUG, "thread %s in %s: suicide timer rearmed", 
            m_id.get().c_str(), m_source->uri().c_str());
        m_suicide.stop();
        m_suicide.start(config_t::get().engine.suicide_timeout);
    }

    return result;
}

void overseer_t::reap(const std::string& driver_id) {
    slave_map_t::iterator it(m_slaves.find(driver_id));

    if(it == m_slaves.end()) {
        syslog(LOG_ERR, "thread %s in %s: [%s()] orphan - driver %s", __func__,
            m_id.get().c_str(), m_source->uri().c_str(), driver_id.c_str());
        return;
    }

    m_slaves.erase(it);

    if(m_slaves.empty()) {
        syslog(LOG_DEBUG, "thread %s in %s: suicide timer started", 
            m_id.get().c_str(), m_source->uri().c_str());
        m_suicide.start(config_t::get().engine.suicide_timeout);
    }
}

void overseer_t::terminate() {
    m_slaves.clear();
    m_source.reset();
    m_loop.unloop();
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
    syslog(LOG_DEBUG, "thread %s in %s: starting", m_id.get().c_str(), source->uri().c_str());

    try {
        m_thread.reset(new boost::thread(
            boost::bind(&overseer_t::run, m_overseer.get(), source)));
    } catch(const boost::thread_resource_error& e) {
        throw std::runtime_error("system thread limit reached");
    }
}

void thread_t::push(core::future_t* future, const std::string& target, const Json::Value& args) {
    m_downstream.send_multi(boost::make_tuple(
        PUSH,
        future->id(),
        target,
        args));
}

void thread_t::drop(core::future_t* future, const std::string& target, const Json::Value& args) {
    m_downstream.send_multi(boost::make_tuple(
        DROP,
        future->id(),
        target,
        args["key"].asString()));
}

void thread_t::track() {

}
