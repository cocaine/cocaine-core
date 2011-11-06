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

        void signal(ev::child& w, int revents);

    private:
        virtual void kill();

    private:
        pid_t m_pid;
        ev::child m_child_watcher;
};

}}}

#endif
