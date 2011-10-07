#include <boost/assign.hpp>

#include "cocaine/overseer.hpp"
#include "cocaine/drivers.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;
using namespace cocaine::plugin;
using namespace cocaine::storage;
using namespace cocaine::helpers;

overseer_t::overseer_t(zmq::context_t& context, const auto_uuid_t& engine_id, const auto_uuid_t& thread_id):
    m_context(context),
    m_link(m_context, ZMQ_DEALER),
    m_loop(),
    m_request(m_loop),
    m_timeout(m_loop),
#if BOOST_VERSION >= 103500
    m_heartbeat(m_loop),
#endif
    m_id(thread_id)
{
    // The routing will be done internally by ZeroMQ, thus the socket identity setup
    m_link.setsockopt(ZMQ_IDENTITY, m_id.get().data(), m_id.get().length());

    // This is set up to avoid very long queues in server mode
    m_link.setsockopt(ZMQ_HWM, &config_t::get().engine.queue_depth,
        sizeof(config_t::get().engine.queue_depth));
        
    // Connect to the engine's controlling socket and set the socket watcher
    m_link.connect("inproc://engine/" + engine_id.get());

    m_request.set<overseer_t, &overseer_t::request>(this);
    m_request.start(m_link.fd(), EV_READ);

    // Initializing suicide timer
    m_timeout.set<overseer_t, &overseer_t::timeout>(this);
    m_timeout.start(config_t::get().engine.suicide_timeout);

#if BOOST_VERSION >= 103500
    // Initialize heartbeat timer
    m_heartbeat.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat.start(5.0, 5.0);
#endif

    // Set timer compression threshold
    m_loop.set_timeout_collect_interval(config_t::get().engine.collect_timeout);

    // Signal a false event, in case the core has managed to send something already
    m_loop.feed_fd_event(m_link.fd(), EV_READ);
}

void overseer_t::run(boost::shared_ptr<source_t> source) {
    m_source = source;
    m_loop.loop();
}

void overseer_t::request(ev::io& w, int revents) {
    static std::map<const std::string, unsigned int> types = boost::assign::map_list_of
        ("auto", AUTO)
        ("manual", MANUAL)
        ("fs", FILESYSTEM)
        ("sink", SINK)
        ("server", SERVER);

    unsigned int code = 0;

    while(m_link.pending()) {
        Json::Value result(Json::objectValue);
        
        // Get the message code
        m_link.recv(code);

        switch(code) {
            case PUSH: {
                Json::Value args;

                m_link.recv(args);

                const std::string type(args["driver"].asString());

                try {
                    if(types.find(type) == types.end()) {
                        throw std::runtime_error("invalid driver type");
                    }

                    switch(types[type]) {
                        case AUTO:
                            result = push<drivers::auto_t>(args);
                            break;
                        case MANUAL:
                            result = push<drivers::manual_t>(args);
                            break;
                        case FILESYSTEM:
                            result = push<drivers::fs_t>(args);
                            break;
                        case SINK:
                            result = push<drivers::sink_t>(args);
                            break;
                        case SERVER:
                            result = push<drivers::server_t>(args);
                            break;
                    }
                } catch(const std::runtime_error& e) {
                    syslog(LOG_ERR, "thread %s: [%s()] %s", m_id.get().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                break;
            }
            case DROP: {
                std::string driver_id;
                
                m_link.recv(driver_id);

                try {
                    result = drop(driver_id);
                } catch(const std::runtime_error& e) {
                    syslog(LOG_ERR, "thread %s in %s: [%s()] %s", m_id.get().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                break;
            }
            case TERMINATE:
                terminate();
                return;
        }
       
        // Report to the core
        m_link.send_multi(boost::make_tuple(
            FUTURE,
            result));
    }
}
 
void overseer_t::timeout(ev::timer& w, int revents) {
    m_link.send(SUICIDE);
}

#if BOOST_VERSION >= 103500
void overseer_t::heartbeat(ev::timer& w, int revents) {
    m_link.send(HEARTBEAT);
}
#endif

template<class DriverType>
Json::Value overseer_t::push(const Json::Value& args) {
    Json::Value result(Json::objectValue);

    std::auto_ptr<DriverType> driver(new DriverType(shared_from_this(), args));
    std::string driver_id(driver->id());
    
    // Scheduling
    if(m_slaves.find(driver_id) == m_slaves.end()) {
        driver->start();
        m_slaves.insert(driver_id, driver);

        if(m_timeout.is_active()) {
            syslog(LOG_DEBUG, "thread %s: suicide timer stopped", m_id.get().c_str());
            m_timeout.stop();
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
            syslog(LOG_ERR, "thread %s: [%s()] storage failure - %s",
                m_id.get().c_str(), __func__, e.what());
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
            syslog(LOG_DEBUG, "thread %s in %s: suicide timer started", m_id.get().c_str());
            m_timeout.start(config_t::get().engine.suicide_timeout);
        }

        // Un-persist
        std::string object_id(m_digest.get(m_id.get() + driver_id));
        
        try {
            storage_t::instance()->remove("tasks", object_id);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "thread %s in %s: [%s()] storage failure - %s",
                m_id.get().c_str(), __func__, e.what());
        }
    } else {
        throw std::runtime_error("driver is not active");
    }

    Json::Value result(Json::objectValue);
    result["status"] = "success";

    return result;
}

void overseer_t::terminate() {
    m_slaves.clear();
    m_source.reset();
    m_loop.unloop();
} 

