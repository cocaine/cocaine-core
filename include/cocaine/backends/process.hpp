#ifndef COCAINE_BACKENDS_PROCESS_HPP
#define COCAINE_BACKENDS_PROCESS_HPP

#include "cocaine/backends/abstract.hpp"

namespace cocaine { namespace engine {

// Applcation Engine Worker
class process_t:
    public backend_t
{
    public:        
        process_t(boost::shared_ptr<engine_t> parent,
                  boost::shared_ptr<plugin::source_t> source);
        virtual ~process_t();

    private:
        virtual void timeout(ev::timer& w, int revents);

    private:
        pid_t m_pid;
};

}}

#endif
