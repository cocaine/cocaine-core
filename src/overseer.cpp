#include "cocaine/overseer.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;
using namespace cocaine::plugin;
using namespace cocaine::storage;
using namespace cocaine::helpers;

overseer_t::overseer_t(unique_id_t::type id_, unique_id_t::type engine_id, zmq::context_t& context):
    unique_id_t(id_),
    m_context(context),
    m_channel(m_context, ZMQ_DEALER, id()),
    m_loop(),
    m_request(m_loop),
    m_timeout(m_loop),
    m_heartbeat(m_loop)
{
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
    m_source = source;
    m_loop.loop();
}

void overseer_t::request(ev::io& w, int revents) {
    unsigned int code = 0;

    while((revents & ev::READ) && m_channel.pending()) {
        Json::Value result;
        m_channel.recv(code);

        switch(code) {
            case PROCESS: {
                std::string blob;

                m_channel.recv(blob);

                try {
                    result = process(blob);
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "overseer %s: [%s()] in process - %s", id().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }
                
                m_channel.send_multi(boost::make_tuple(
                    FUTURE,
                    result)); 
                
                break;
            }

            case INVOKE: {
                std::string task;

                m_channel.recv(task);

                try {
                    result = invoke(task);
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "overseer %s: [%s()] %s", id().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                if(!result.isNull()) {
                    m_channel.send_multi(boost::make_tuple(
                        EVENT,
                        task,
                        result)); 
                }
                
                break;
            }
            
            case TERMINATE:
                terminate();
                return;
        }
    }
}
 
void overseer_t::timeout(ev::timer& w, int revents) {
    m_channel.send(SUICIDE);
    terminate();
}

void overseer_t::heartbeat(ev::timer& w, int revents) {
    m_channel.send(HEARTBEAT);
}

Json::Value overseer_t::process(const std::string& blob) {
    Json::Value result(m_source->process(blob.data(), blob.size()));

    m_timeout.stop();
    m_timeout.start(config_t::get().engine.suicide_timeout);

    return result;
}

Json::Value overseer_t::invoke(const std::string& task) {
    Json::Value result(m_source->invoke(task));

    m_timeout.stop();
    m_timeout.start(config_t::get().engine.suicide_timeout);

    return result;
}

void overseer_t::terminate() {
    m_source.reset();
    m_loop.unloop();
} 

