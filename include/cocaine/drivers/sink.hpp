#ifndef COCAINE_DRIVERS_ZEROMQ_HPP
#define COCAINE_DRIVERS_ZEROMQ_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

class sink_t:
    public driver_base_t<ev::io, sink_t>
{
    public:
        sink_t(boost::shared_ptr<overseer_t> parent, const Json::Value& args):
            driver_base_t<ev::io, sink_t>(parent),
            m_endpoint(args.get("endpoint", "").asString()),
            m_watermark(args.get("watermark", 10).asUInt())
        {
            if(~m_parent->source()->capabilities() & plugin::source_t::PROCESSOR) {
                throw std::runtime_error("source doesn't support message processing");
            }

            if(m_endpoint.empty()) {
                throw std::runtime_error("no endpoint specified");
            }

            m_id = "sink:" + digest_t().get(m_parent->source()->uri() + m_endpoint);
        }

        virtual void operator()(ev::io&, int) {
            zmq::message_t message;
            Json::Value result; 

            while(m_socket->pending()) {
                m_socket->recv(&message);

                try {
                    result = m_parent->source()->process(message.data(), message.size());
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "driver %s [%s]: [%s()] %s",
                        m_id.c_str(), m_parent->id().c_str(), __func__, e.what());
                    result["error"] = e.what();
                }

                publish(result);
            }
        }

        void initialize() {
            m_socket.reset(new lines::socket_t(m_parent->context(), ZMQ_PULL));
            
            m_socket->setsockopt(ZMQ_HWM, &m_watermark, sizeof(m_watermark));
            m_socket->bind(m_endpoint);

            m_watcher->set(m_socket->fd(), EV_READ);
        }

    private:
        std::string m_endpoint;
#if ZMQ_VERSION > 30000
        int m_watermark;
#else
        uint64_t m_watermark;
#endif

        std::auto_ptr<lines::socket_t> m_socket;
};

}}}

#endif
