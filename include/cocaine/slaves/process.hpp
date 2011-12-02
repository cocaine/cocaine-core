#ifndef COCAINE_SLAVE_PROCESS_HPP
#define COCAINE_SLAVE_PROCESS_HPP

#include "cocaine/slaves/base.hpp"

namespace cocaine { namespace engine { namespace slave {

class process_t:
    public slave_t
{
    public:
        process_t(engine_t* engine,
                  const std::string& type,
                  const std::string& args);

        virtual void reap();

    private:
        void signal(ev::child&, int);

    private:
        pid_t m_pid;
        ev::child m_child_watcher;
};

}}}

#endif
