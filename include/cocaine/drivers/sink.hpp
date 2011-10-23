#ifndef COCAINE_DRIVERS_SINK_HPP
#define COCAINE_DRIVERS_SINK_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

class sink_t:
    public driver_base_t<ev::io, sink_t>
{
    public:
        sink_t(const std::string& name, boost::shared_ptr<engine_t> parent, const Json::Value& args):
            driver_base_t<ev::io, sink_t>(name, parent),
            m_endpoint(args.get("endpoint", "").asString())
        {
            if(m_endpoint.empty()) {
                throw std::runtime_error("no endpoint has been specified for the sink driver");
            }

            m_id = "sink:" + security::digest_t().get(m_name + m_endpoint);
        }

        virtual void operator()(ev::io&, int revents) {
            zmq::message_t message;

            while((revents & ev::READ) && m_socket->pending()) {
                m_socket->recv(&message);
            
                boost::shared_ptr<lines::publication_t> publication(
                    new lines::publication_t(m_name, m_parent));

                try {
                    publication->wait(m_parent->queue(
                        boost::make_tuple(
                            INVOKE,
                            m_name,
                            boost::ref(message)
                        )
                    ));
                } catch(const std::runtime_error& e) {
                    syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                        m_parent->name().c_str(), m_name.c_str(), e.what());
                }
            }
        }

        void initialize() {
            m_socket.reset(new lines::socket_t(m_parent->context(), ZMQ_PULL));
            m_socket->bind(m_endpoint);
            m_watcher->set(m_socket->fd(), ev::READ);
        }

    private:
        const std::string m_endpoint;
        std::auto_ptr<lines::socket_t> m_socket;
};

}}}

#endif
