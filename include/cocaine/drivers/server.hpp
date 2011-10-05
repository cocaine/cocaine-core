#ifndef COCAINE_DRIVERS_SERVER_HPP
#define COCAINE_DRIVERS_SERVER_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

class server_t:
    public driver_base_t<ev::io, server_t>
{
    public:
        server_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            driver_base_t<ev::io, server_t>(parent, source),
            m_endpoint(args.get("endpoint", "").asString())
        {
            if(~m_source->capabilities() & plugin::source_t::PROCESSOR) {
                throw std::runtime_error("source doesn't support message processing");
            }

            if(m_endpoint.empty()) {
                throw std::runtime_error("no endpoint specified");
            }

            m_id = "server:" + digest_t().get(m_source->uri() + m_endpoint);
        }

        virtual void operator()(ev::io&, int) {
            zmq::message_t message;
            Json::Value result; 

            while(m_socket->pending()) {
                // Fetch the request
                m_socket->recv(&message);

                // Process it
                try {
                    result = m_source->process(message.data(), message.size());
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "driver %s in thread %s in %s: [%s()] %s",
                        m_id.c_str(), m_parent->id().c_str(),
                        m_source->uri().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                // And respond
                std::string response(m_writer.write(result));
                message.rebuild(response.length());
                memcpy(message.data(), response.data(), response.length());

                m_socket->send(message);
            }
        }

        void initialize() {
            m_socket.reset(new lines::socket_t(m_parent->context(), ZMQ_REP));
            m_socket->bind(m_endpoint);
            m_watcher->set(m_socket->fd(), EV_READ);
        }

    private:
        std::string m_endpoint;
        std::auto_ptr<lines::socket_t> m_socket;
        Json::FastWriter m_writer;
};

}}}

#endif
