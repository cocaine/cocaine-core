#include "cocaine/drivers/native_server.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

native_server_job_t::native_server_job_t(native_server_t* driver, const messages::request_t& request, const route_t& route):
    unique_id_t(request.id),
    job::job_t(driver, request.policy),
    m_route(route)
{ }

void native_server_job_t::react(const events::response& event) {
    send(messages::tag_t(id()), ZMQ_SNDMORE);
    
    zeromq_server_t* server = static_cast<zeromq_server_t*>(m_driver);
    server->socket().send(event.message);
}

void native_server_job_t::react(const events::error& event) {
    send(messages::error_t(id(), event.code, event.message));
}

void native_server_job_t::react(const events::completed& event) {
    send(messages::tag_t(id(), true));
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
            unsigned int type = 0;
            messages::request_t request;

            boost::tuple<unsigned int&, messages::request_t&> tier(type, request);

            if(!m_socket.recv_multi(tier)) {
                syslog(LOG_ERR, "driver [%s:%s]: got a corrupted request from '%s'",
                    m_engine->name().c_str(), m_method.c_str(), route.back().c_str());
                continue;
            }

            // TEST: This is temporary for testing purposes
            BOOST_ASSERT(type == request.type);

            boost::shared_ptr<native_server_job_t> job;
            
            try {
                job.reset(new native_server_job_t(this, request, route));
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "driver [%s:%s]: got a corrupted request from '%s' - %s",
                    m_engine->name().c_str(), m_method.c_str(), route.back().c_str(), e.what());
                continue;
            }

            if(!m_socket.recv(job->request(), ZMQ_NOBLOCK)) {
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

