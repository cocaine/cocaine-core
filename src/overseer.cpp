#include "cocaine/overseer.hpp"
#include "cocaine/drivers.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;
using namespace cocaine::plugin;
using namespace cocaine::storage;
using namespace cocaine::helpers;

overseer_t::overseer_t(unique_id_t::type id_, unique_id_t::type engine_id, zmq::context_t& context):
    unique_id_t(id_),
    m_context(context),
    m_channel(m_context, ZMQ_DEALER),
    m_loop(),
    m_request(m_loop),
    m_timeout(m_loop),
    m_heartbeat(m_loop),
    m_isolated(id_ != engine_id)
{
    // The routing will be done internally by ZeroMQ, thus the socket identity setup
    m_channel.setsockopt(ZMQ_IDENTITY, id().data(), id().length());

    // This is set up to avoid very long queues in server mode
    m_channel.setsockopt(ZMQ_HWM, &config_t::get().engine.queue_depth,
        sizeof(config_t::get().engine.queue_depth));
        
    // Connect to the engine's controlling socket and set the socket watcher
    m_channel.connect("inproc://engine/" + engine_id);

    m_request.set<overseer_t, &overseer_t::request>(this);
    m_request.start(m_channel.fd(), EV_READ);

    // Initializing suicide timer
    m_timeout.set<overseer_t, &overseer_t::timeout>(this);
    m_timeout.start(config_t::get().engine.suicide_timeout);

    // Initialize heartbeat timer
    m_heartbeat.set<overseer_t, &overseer_t::heartbeat>(this);
    m_heartbeat.start(5.0, 5.0);

    // Set timer compression threshold
    m_loop.set_timeout_collect_interval(config_t::get().engine.collect_timeout);

    // Signal a false event, in case the core has managed to send something already
    m_loop.feed_fd_event(m_channel.fd(), EV_READ);
}

void overseer_t::run(boost::shared_ptr<source_t> source) {
    syslog(LOG_DEBUG, "overseer %s: thread id is 0x%lx", id().c_str(), pthread_self());

    m_source = source;
    m_loop.loop();
}

void overseer_t::request(ev::io& w, int revents) {
    unsigned int code = 0;

    while(m_channel.pending()) {
        Json::Value result(Json::objectValue);
        
        // Get the message code
        m_channel.recv(code);

        switch(code) {
            case PUSH: {
                unsigned int type = 0;
                Json::Value args;

                boost::tuple<unsigned int&, Json::Value&> tier(type, args);
                m_channel.recv_multi(tier);

                try {
                    switch(type) {
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
                    syslog(LOG_ERR, "overseer %s: [%s()] %s", id().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                break;
            }

            case DROP: {
                std::string driver_id;
                
                m_channel.recv(driver_id);

                try {
                    result = drop(driver_id);
                } catch(const std::runtime_error& e) {
                    syslog(LOG_ERR, "overseer %s: [%s()] %s", id().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                break;
            }

            case ONCE: {
                Json::Value o;
                m_channel.recv(o);
                sleep(5);
                result["yes"] = "no";
                break;
            }

            case TERMINATE:
                terminate();
                return;
        }
       
        // Report to the core
        m_channel.send_multi(boost::make_tuple(
            FUTURE,
            result)); 
    }
}
 
void overseer_t::timeout(ev::timer& w, int revents) {
    m_channel.send(SUICIDE);
}

void overseer_t::heartbeat(ev::timer& w, int revents) {
    m_channel.send(HEARTBEAT);
}

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
            syslog(LOG_DEBUG, "overseer %s: suicide timer stopped", id().c_str());
            m_timeout.stop();
        }
    }
   
    result["key"] = driver_id;

    // Persistance
    if(!args.get("transient", false).asBool()) {
        try { 
            if(!storage_t::instance()->exists("tasks", driver_id)) {
                Json::Value object(args);

                if(m_isolated)
                    object["thread"] = id();

                storage_t::instance()->put("tasks", driver_id, object);
            }
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "overseer %s: [%s()] storage failure - %s",
                id().c_str(), __func__, e.what());
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
            syslog(LOG_DEBUG, "overseer %s: suicide timer started", id().c_str());
            m_timeout.start(config_t::get().engine.suicide_timeout);
        }

        // Un-persist
        try {
            storage_t::instance()->remove("tasks", driver_id);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "overseer %s: [%s()] storage failure - %s",
                id().c_str(), __func__, e.what());
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
    m_request.stop();
    m_timeout.stop();
    m_heartbeat.stop();
    m_source.reset();
    m_loop.unloop();
} 

