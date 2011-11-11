#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/drivers/server+zmq.hpp"

using namespace cocaine::engine::drivers;
using namespace cocaine::lines;

zmq_job_t::zmq_job_t(zmq_server_t* server, const route_t& route):
    job_t(server),
    m_route(route)
{ }

void zmq_job_t::enqueue() {
    zmq::message_t request;

    request.copy(&m_request);

    m_parent->engine()->enqueue(
        shared_from_this(),
        boost::make_tuple(
            INVOKE,
            m_parent->method(),
            boost::ref(request)));
}

void zmq_job_t::send(zmq::message_t& chunk) {
    static_cast<zmq_server_t*>(m_parent)->send(this, chunk);
}

void zmq_job_t::send(error_code code, const std::string& error) {
    Json::Value object(Json::objectValue);
    
    object["code"] = code;
    object["error"] = error;

    Json::FastWriter writer;
    std::string response(writer.write(object));

    zmq::message_t message(response.size());
    memcpy(message.data(), response.data(), response.size());

    static_cast<zmq_server_t*>(m_parent)->send(this, message);
}

zmq_server_t::zmq_server_t(engine_t* engine, const std::string& method, const Json::Value& args):
    driver_t(engine, method),
    m_socket(m_engine->context(), ZMQ_ROUTER, boost::algorithm::join(
        boost::assign::list_of
            (config_t::get().core.instance)
            (config_t::get().core.hostname)
            (m_engine->name())
            (method),
        "/"))
{
    std::string endpoint(args.get("endpoint", "").asString());

    if(endpoint.empty()) {
        throw std::runtime_error("no endpoint has been specified for the '" + m_method + "' task");
    }
    
    m_socket.bind(endpoint);

    m_watcher.set(this);
    m_watcher.start(m_socket.fd(), ev::READ);
    m_processor.set<zmq_server_t, &zmq_server_t::process>(this);
    m_processor.start();
}

zmq_server_t::~zmq_server_t() {
    m_watcher.stop();
    m_processor.stop();
}

Json::Value zmq_server_t::info() const {
    Json::Value result(Json::objectValue);

    result["type"] = "server+zmq";
    result["spent"] = m_spent;
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void zmq_server_t::operator()(ev::io&, int) {
    if(m_socket.pending()) {
        m_watcher.stop();
        m_processor.start();
    }
}

void zmq_server_t::process(ev::idle&, int) {
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

        boost::shared_ptr<zmq_job_t> job(new zmq_job_t(this, route));

        if(m_socket.more()) {
#if ZMQ_VERSION < 30000
            m_socket.recv(&job->request());
#else
            job->request().copy(&message);
#endif
        } else {
            job->send(request_error, "missing request");
            return;
        }

        try {
            job->enqueue();
        } catch(const resource_error_t& e) {
            syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                m_engine->name().c_str(), m_method.c_str(), e.what());
            job->send(resource_error, e.what());
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                m_engine->name().c_str(), m_method.c_str(), e.what());
            job->send(server_error, e.what());
        }
    } else {
        m_watcher.start(m_socket.fd(), ev::READ);
        m_processor.stop();
    }
}

void zmq_server_t::send(zmq_job_t* job, zmq::message_t& chunk) {
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

    m_socket.send(chunk);
}

