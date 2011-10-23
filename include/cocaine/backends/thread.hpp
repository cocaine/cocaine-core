#ifndef COCAINE_BACKENDS_THREAD_HPP
#define COCAINE_BACKENDS_THREAD_HPP

#include <boost/thread.hpp>

#include <cocaine/backends/abstract.hpp>

namespace cocaine { namespace engine {

class thread_t:
    public backend_t
{
    public:        
        thread_t(boost::shared_ptr<engine_t> parent,
                 boost::shared_ptr<plugin::source_t> source);
        virtual ~thread_t();

    private:
        virtual void timeout(ev::timer& w, int revents);

    private:
        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;
};

}}

#endif
