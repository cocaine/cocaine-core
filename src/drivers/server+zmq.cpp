#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/drivers/server+zmq.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::driver;
using namespace cocaine::lines;

zmq_job_t::zmq_job_t(zmq_server_t* driver, job::policy_t policy, const route_t& route):
    job_t(driver, policy),
    m_route(route)
{ }

void zmq_job_t::react(const events::response& event) {
    send(event.message);
}

void zmq_job_t::react(const events::error& event) {
    Json::Value object(Json::objectValue);
    
    object["code"] = event.code;
    object["message"] = event.message;

    std::string response(Json::FastWriter().write(object));
    zmq::message_t message(response.size());
    memcpy(message.data(), response.data(), response.size());

    send(message);
}

void zmq_job_t::send(zmq::message_t& chunk) {
    zmq::message_t message;
    zmq_server_t* server = static_cast<zmq_server_t*>(m_driver);
    
    // Send the identity
    for(route_t::const_iterator id = m_route.begin(); id != m_route.end(); ++id) {
        message.rebuild(id->size());
        memcpy(message.data(), id->data(), id->size());
        server->socket().send(message, ZMQ_SNDMORE);
    }

    // Send the delimiter
    message.rebuild(0);
    server->socket().send(message, ZMQ_SNDMORE);

    // Send the chunk
    server->socket().send(chunk);
}

zmq_server_t::zmq_server_t(engine_t* engine, const std::string& method, const Json::Value& args) try:
    driver_t(engine, method),
    m_socket(m_engine->context(), ZMQ_ROUTER, boost::algorithm::join(
        boost::assign::list_of
            (config_t::get().core.instance)
            (config_t::get().core.hostname)
            (m_engine->name())
            (method),
        "/")
    )
{
    std::string endpoint(args.get("endpoint", "").asString());

    if(endpoint.empty()) {
        throw std::runtime_error("no endpoint has been specified for the '" + m_method + "' task");
    }
    
    m_socket.bind(endpoint);

    m_watcher.set<zmq_server_t, &zmq_server_t::event>(this);
    m_watcher.start(m_socket.fd(), ev::READ);
    m_processor.set<zmq_server_t, &zmq_server_t::process>(this);
    m_processor.start();
} catch(const zmq::error_t& e) {
    throw std::runtime_error("network failure in '" + m_method + "' task - " + e.what());
}

zmq_server_t::~zmq_server_t() {
    m_watcher.stop();
    m_processor.stop();
}

Json::Value zmq_server_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "server+zmq";
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void zmq_server_t::event(ev::io&, int) {
    if(m_socket.pending()) {
        m_watcher.stop();
        m_processor.start();
    }
}

void zmq_server_t::process(ev::idle&, int) {
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

        boost::shared_ptr<zmq_job_t> job(new zmq_job_t(this, job::policy_t(), route));

        if(m_socket.more()) {
            m_socket.recv(job->request());
        } else {
            job->process_event(events::request_error("missing request body"));
            return;
        }

        m_engine->enqueue(job);
    } else {
        m_watcher.start(m_socket.fd(), ev::READ);
        m_processor.stop();
    }
}

