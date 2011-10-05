#ifndef COCAINE_DRIVERS_ZEROMQ_HPP
#define COCAINE_DRIVERS_ZEROMQ_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

class sink_t:
    public driver_base_t<ev::io, sink_t>
{
    public:
        sink_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            driver_base_t<ev::io, sink_t>(parent, source),
            m_endpoint(args.get("endpoint", "").asString())
        {
            if(~m_source->capabilities() & plugin::source_t::PROCESSOR) {
                throw std::runtime_error("source doesn't support message processing");
            }

            if(m_endpoint.empty()) {
                throw std::runtime_error("no endpoint specified");
            }

            m_id = "sink:" + digest_t().get(m_source->uri() + m_endpoint);
        }

        virtual void operator()(ev::io&, int) {
            zmq::message_t message;
            Json::Value result; 

            while(m_sink->pending()) {
                m_sink->recv(&message);

                try {
                    result = m_source->process(message.data(), message.size());
                } catch(const std::exception& e) {
                    syslog(LOG_ERR, "engine: error in %s driver - %s",
                        m_id.c_str(), e.what());
                    result["error"] = e.what();
                }

                publish(result);
            }
        }

        void initialize() {
            m_sink.reset(new lines::socket_t(m_parent->context(), ZMQ_PULL));
            m_sink->bind(m_endpoint);
            m_watcher->set(m_sink->fd(), EV_READ);
        }

    private:
        std::string m_endpoint;
        std::auto_ptr<lines::socket_t> m_sink;
};

}}}

#endif
