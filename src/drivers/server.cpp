#include "cocaine/drivers/server.hpp"

using namespace cocaine::engine::drivers;

response_t::response_t(const lines::route_t& route, boost::shared_ptr<responder_t> parent):
    m_route(route),
    m_parent(parent)
{ }

void response_t::send(zmq::message_t& chunk) {
    m_parent->respond(m_route, chunk);
}

void response_t::abort(const std::string& error) {
    Json::Value object(Json::objectValue);

    object["error"] = error;
    
    Json::FastWriter writer;
    std::string response(writer.write(object));
    zmq::message_t message(response.size());
    
    memcpy(message.data(), response.data(), response.size());
    
    m_parent->respond(m_route, message);
}

server_t::server_t(const std::string& method, boost::shared_ptr<engine_t> parent, const Json::Value& args):
    driver_t(method, parent),
    m_socket(parent->context(), ZMQ_ROUTER, 
        config_t::get().core.hostname + "/" + 
        config_t::get().core.instance + "/" + 
        m_parent->name() + "/" + 
        method)
{
    std::string endpoint(args.get("endpoint", "").asString());

    if(endpoint.empty()) {
        throw std::runtime_error("no endpoint has been specified");
    }
    
    m_socket.bind(endpoint);

    m_watcher.set(this);
    m_watcher.start(m_socket.fd(), ev::READ);

    m_processor.set<server_t, &server_t::process>(this);
    m_processor.start();
}

void server_t::operator()(ev::io&, int) {
    if(m_socket.pending()) {
        m_processor.start();
        m_watcher.stop();
    }
}

void server_t::process(ev::idle&, int) {
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

#if ZMQ_VERSION < 30000
        m_socket.recv(&message);
#endif

        boost::shared_ptr<response_t> deferred(
            new response_t(route, shared_from_this()));

        try {
            m_parent->queue(
                deferred,
                boost::make_tuple(
                    INVOKE,
                    m_method,
                    boost::ref(message)));
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                m_parent->name().c_str(), m_method.c_str(), e.what());
            deferred->abort(e.what());
        }
    } else {
        m_processor.stop();
        m_watcher.start(m_socket.fd(), ev::READ);
    }
}

Json::Value server_t::info() const {
    Json::Value result(Json::objectValue);

    result["type"] = "server";
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void server_t::respond(const lines::route_t& route, zmq::message_t& chunk) {
    zmq::message_t message;
    
    // Send the identity
    for(lines::route_t::const_iterator id = route.begin(); id != route.end(); ++id) {
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
