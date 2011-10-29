#include "cocaine/drivers/server.hpp"

using namespace cocaine::engine::drivers;
using namespace cocaine::lines;

response_t::response_t(const route_t& route, boost::shared_ptr<responder_t> responder):
    m_route(route),
    m_responder(responder)
{ }

void response_t::send(zmq::message_t& chunk) {
    m_responder->respond(m_route, chunk);
}

void response_t::abort(const std::string& error) {
    Json::Value object(Json::objectValue);

    object["error"] = error;
    
    Json::FastWriter writer;
    std::string response(writer.write(object));
    zmq::message_t message(response.size());
    
    memcpy(message.data(), response.data(), response.size());
    
    m_responder->respond(m_route, message);
}

server_t::server_t(const std::string& method, boost::shared_ptr<engine_t> engine, const Json::Value& args):
    driver_t(method, engine),
    m_socket(m_engine->context(), ZMQ_ROUTER, 
        config_t::get().core.hostname + "/" + 
        config_t::get().core.instance + "/" + 
        m_engine->name() + "/" + 
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

server_t::~server_t() {
    pause();
}

void server_t::pause() {
    m_watcher.stop();
    m_processor.stop();
}

void server_t::resume() {
    m_watcher.start();
    m_processor.start();
}

Json::Value server_t::info() const {
    Json::Value result(Json::objectValue);

    result["type"] = "server";
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void server_t::operator()(ev::io&, int) {
    if(m_socket.pending()) {
        m_watcher.stop();
        m_processor.start();
    }
}

void server_t::respond(const route_t& route, zmq::message_t& chunk) {
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
            m_engine->enqueue(
                deferred,
                boost::make_tuple(
                    INVOKE,
                    m_method,
                    boost::ref(message)));
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                m_engine->name().c_str(), m_method.c_str(), e.what());
            deferred->abort(e.what());
        }
    } else {
        m_watcher.start(m_socket.fd(), ev::READ);
        m_processor.stop();
    }
}

