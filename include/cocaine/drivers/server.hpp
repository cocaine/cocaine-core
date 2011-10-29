#ifndef COCAINE_DRIVERS_SERVER_HPP
#define COCAINE_DRIVERS_SERVER_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class responder_t {
    public:
        virtual void respond(const lines::route_t& route, zmq::message_t& chunk) = 0;
};

class response_t:
    public lines::deferred_t
{
    public:
        response_t(const lines::route_t& route, boost::shared_ptr<responder_t> parent);

    public:
        virtual void send(zmq::message_t& chunk);
        virtual void abort(const std::string& error);

    private:
        const lines::route_t m_route;
        const boost::shared_ptr<responder_t> m_parent;
};

class server_t:
    public boost::enable_shared_from_this<server_t>,
    public driver_t,
    public responder_t
{
    public:
        server_t(const std::string& method, 
                 boost::shared_ptr<engine_t> parent,
                 const Json::Value& args);
        virtual ~server_t();

    public:
        virtual void pause();
        virtual void resume();

        virtual Json::Value info() const;
        
        virtual void respond(const lines::route_t& route, zmq::message_t& chunk);

        void operator()(ev::io&, int);
        void process(ev::idle&, int);

    private:
        lines::socket_t m_socket;
        ev::io m_watcher; 
        ev::idle m_processor;
};

}}}

#endif
