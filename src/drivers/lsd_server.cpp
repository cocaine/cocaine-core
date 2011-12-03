#include "cocaine/drivers/lsd_server.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

lsd_job_t::lsd_job_t(lsd_server_t* driver, job::policy_t policy, const unique_id_t::type& id, const route_t& route):
    job::job_t(driver, policy),
    unique_id_t(id),
    m_route(route)
{ }

void lsd_job_t::react(const events::response& event) {
    Json::Value root(Json::objectValue);
    
    root["uuid"] = id();
    
    send(root, event.message);
}

void lsd_job_t::react(const events::error& event) {
    zmq::message_t null;
    Json::Value root(Json::objectValue);

    root["uuid"] = id();
    root["code"] = event.code;
    root["message"] = event.message;

    send(root, null);
}

void lsd_job_t::react(const events::completed& event) {
    zmq::message_t null;
    Json::Value root(Json::objectValue);

    root["uuid"] = id();
    root["completed"] = true;

    send(root, null);
}

void lsd_job_t::send(const Json::Value& root, zmq::message_t& chunk) {
    zmq::message_t message;
    zeromq_server_t* server = static_cast<zeromq_server_t*>(m_driver);

    // Send the identity
    for(route_t::const_iterator id = m_route.begin(); id != m_route.end(); ++id) {
        message.rebuild(id->size());
        memcpy(message.data(), id->data(), id->size());
        server->socket().send(message, ZMQ_SNDMORE);
    }

    // Send the delimiter
    message.rebuild(0);
    server->socket().send(message, ZMQ_SNDMORE);

    // Send the envelope
    std::string envelope(Json::FastWriter().write(root));
    message.rebuild(envelope.size());
    memcpy(message.data(), envelope.data(), envelope.size());

    if(chunk.size()) {
        server->socket().send(message, ZMQ_SNDMORE);
        server->socket().send(chunk);
    } else {
        server->socket().send(message);
    }
}

lsd_server_t::lsd_server_t(engine_t* engine, const std::string& method, const Json::Value& args):
    zeromq_server_t(engine, method, args)
{ }

Json::Value lsd_server_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "server+lsd";
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void lsd_server_t::process(ev::idle&, int) {
    if(m_socket.pending()) {
        zmq::message_t message;
        route_t route;

        while(true) {
            m_socket.recv(&message);

            if(!message.size()) {
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

        while(m_socket.more()) {
            // Receive the nvelope
            m_socket.recv(&message);

            // Parse the envelope and setup the job policy
            Json::Reader reader(Json::Features::strictMode());
            Json::Value root;

            if(!reader.parse(
                static_cast<const char*>(message.data()),
                static_cast<const char*>(message.data()) + message.size(),
                root))
            {
                syslog(LOG_ERR, "driver [%s:%s]: invalid envelope - %s",
                    m_engine->name().c_str(), m_method.c_str(), reader.getFormatedErrorMessages().c_str());
                m_socket.drop_remaining_parts();
                break;
            }

            job::policy_t policy(
                root.get("urgent", false).asBool(),
                root.get("timeout", config_t::get().engine.heartbeat_timeout).asDouble(),
                root.get("deadline", 0.0f).asDouble());

            boost::shared_ptr<lsd_job_t> job;
            
            try {
                job.reset(new lsd_job_t(this, policy, root.get("uuid", "").asString(), route));
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "driver [%s:%s]: invalid envelope - %s",
                    m_engine->name().c_str(), m_method.c_str(), e.what());
                m_socket.drop_remaining_parts();
                break;
            }

            if(m_socket.more()) {
                m_socket.recv(job->request());
            } else {
                job->process_event(events::request_error("missing request body"));
                break;
            }
            
            m_engine->enqueue(job);
        }
    } else {
        m_watcher.start(m_socket.fd(), ev::READ);
        m_processor.stop();
    }
}

