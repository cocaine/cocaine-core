#ifndef COCAINE_DRIVERS_RAW_SERVER_HPP
#define COCAINE_DRIVERS_RAW_SERVER_HPP

#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class raw_server_t:
    public driver_t
{
    public:
        raw_server_t(engine_t* engine,
                     const std::string& method, 
                     const Json::Value& args);
        virtual ~raw_server_t();

        // Driver interface
        virtual void stop();

        // Server interface
        void operator()(ev::io&, int);
        virtual void process(int fd) = 0;

    protected:
        unsigned int m_port;
        unsigned int m_backlog;

        int m_socket;
        ev::io m_watcher; 
};

}}}

#endif
