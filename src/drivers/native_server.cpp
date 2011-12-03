#include "cocaine/drivers/native_server.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

const unsigned int messages::chunk_t::type = 0;
const unsigned int messages::error_t::type = 1;
const unsigned int messages::tag_t::type = 2;

native_server_job_t::native_server_job_t(native_server_t* driver, const messages::request_t& request, const route_t& route):
    unique_id_t(request.id),
    job::job_t(driver, request.policy),
    m_route(route)
{
    job_t::request()->rebuild(request.body.size);
    memcpy(job_t::request()->data(), request.body.ptr, request.body.size);
}

void native_server_job_t::react(const events::response& event) {
    send(messages::chunk_t(id(), event.message));
}

void native_server_job_t::react(const events::error& event) {
    send(messages::error_t(id(), event.code, event.message));
}

void native_server_job_t::react(const events::completed& event) {
    send(messages::tag_t(id()));
}

native_server_t::native_server_t(engine_t* engine, const std::string& method, const Json::Value& args):
    zeromq_server_t(engine, method, args)
{ }

Json::Value native_server_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "native-server";
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void native_server_t::process(ev::idle&, int) {
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
            messages::request_t request;

            if(!m_socket.recv(request)) {
                syslog(LOG_ERR, "driver [%s:%s]: got a corrupted request",
                    m_engine->name().c_str(), m_method.c_str());
                continue;
            }

            boost::shared_ptr<native_server_job_t> job;
            
            try {
                job.reset(new native_server_job_t(this, request, route)); 
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "driver [%s:%s]: got a malformed request - %s - %s",
                    m_engine->name().c_str(), m_method.c_str(), e.what(), request.id.c_str());
                continue;
            }

            m_engine->enqueue(job);
        }
    } else {
        m_watcher.start(m_socket.fd(), ev::READ);
        m_processor.stop();
    }
}

