#include "cocaine/drivers/server+lsd.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::drivers;
using namespace cocaine::lines;

lsd_job_t::lsd_job_t(lsd_server_t* server, const route_t& route):
    zmq_job_t(server, route)
{ }

void lsd_job_t::send(error_code code, const std::string& error) {
    zmq::message_t null;

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(reader.parse(
        static_cast<const char*>(m_envelope.data()),
        static_cast<const char*>(m_envelope.data()) + m_envelope.size(),
        root))
    {
        root["code"] = code;
        root["error"] = error;
    }

    Json::FastWriter writer;
    std::string response(writer.write(root));

    m_envelope.rebuild(response.size());
    memcpy(m_envelope.data(), response.data(), response.size());

    static_cast<lsd_server_t*>(m_parent)->send(this, null);
}

lsd_server_t::lsd_server_t(engine_t* engine, const std::string& method, const Json::Value& args):
    zmq_server_t(engine, method, args)
{ }

Json::Value lsd_server_t::info() const {
    Json::Value result(Json::objectValue);

    result["stats"] = stats();
    result["type"] = "server+lsd";
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void lsd_server_t::process(ev::idle&, int) {
    if(m_socket.pending()) {
        zmq::message_t message;
        std::vector<std::string> route;

        while(true) {
            m_socket.recv(&message);

#if ZMQ_VERSION > 30000
            if(!m_socket.label()) {
#else
            if(!message.size()) {
#endif
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

        boost::shared_ptr<lsd_job_t> job(new lsd_job_t(this, route));

        if(m_socket.more()) {
            // LSD envelope
#if ZMQ_VERSION < 30000
            m_socket.recv(&job->envelope());
#else
            job->envelope().copy(&message);
#endif
        } else {
            job->send(request_error, "missing envelope");
            return;
        }

        if(m_socket.more()) {
            // Request
            m_socket.recv(&job->request());
        } else {
            job->send(request_error, "missing request");
            return;
        }

        // Parse the envelope and setup the job policy
        Json::Reader reader(Json::Features::strictMode());
        Json::Value root;

        if(!reader.parse(
            static_cast<const char*>(job->envelope().data()),
            static_cast<const char*>(job->envelope().data()) + job->envelope().size(),
            root))
        {
            job->send(request_error, "invalid envelope");
            return;
        }

        job_policy policy(
            root.get("urgent", false).asBool(),
            root.get("timeout", config_t::get().engine.heartbeat_timeout).asDouble(),
            root.get("deadline", 0.0f).asDouble());

        // Instantly drop the job if the deadline has already passed
        if(policy.deadline >= ev::get_default_loop().now()) {
            job->send(timeout_error, "the job has expired");
            return;
        }

        // Enqueue
        try {
            job->enqueue_with_policy(policy);
        } catch(const resource_error_t& e) {
            syslog(LOG_ERR, "driver [%s:%s]: unable to enqueue the invocation - %s",
                m_engine->name().c_str(), m_method.c_str(), e.what());
            job->send(resource_error, e.what());
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "driver [%s:%s]: unable to enqueue the invocation - %s",
                m_engine->name().c_str(), m_method.c_str(), e.what());
            job->send(server_error, e.what());
        }
    } else {
        m_watcher.start(m_socket.fd(), ev::READ);
        m_processor.stop();
    }
}

void lsd_server_t::send(zmq_job_t* job, zmq::message_t& chunk) {
    const route_t& route(job->route());
    zmq::message_t message;
    
    // Send the identity
    for(route_t::const_iterator id = route.begin(); id != route.end(); ++id) {
        message.rebuild(id->length());
        memcpy(message.data(), id->data(), id->length());
#if ZMQ_VERSION < 30000
        m_socket.send(message, ZMQ_SNDMORE);
#else
        m_socket.send(message, ZMQ_SNDMORE | ZMQ_SNDLABEL);
#endif
    }

#if ZMQ_VERSION < 30000                
    // Send the delimiter
    message.rebuild(0);
    m_socket.send(message, ZMQ_SNDMORE);
#endif

    message.copy(&static_cast<lsd_job_t*>(job)->envelope());
    
    // Send the response
    if(chunk.size()) {
        m_socket.send(message, ZMQ_SNDMORE);
        m_socket.send(chunk);
    } else {
        m_socket.send(message);
    }
}

