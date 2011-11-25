#ifndef COCAINE_BACKENDS_PROCESS_HPP
#define COCAINE_BACKENDS_PROCESS_HPP

#include "cocaine/backends/abstract.hpp"

namespace cocaine { namespace engine { namespace backends {

class process_t:
    public backend_t
{
    public:        
        process_t(engine_t* engine,
                  const std::string& type,
                  const std::string& args);
        virtual ~process_t();

        virtual void stop();

    private:
        virtual void kill();
        
        void signal(ev::child&, int);
        void terminate(ev::timer&, int);

    private:
        pid_t m_pid;
        ev::child m_child_watcher;
        ev::timer m_termination_timeout;
};

}}}

#endif
