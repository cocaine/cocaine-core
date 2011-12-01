#ifndef COCAINE_SLAVE_THREAD_HPP
#define COCAINE_SLAVE_THREAD_HPP

#include <boost/thread/thread.hpp>

#include <cocaine/slaves/base.hpp>

namespace cocaine { namespace engine { namespace slave {

class thread_t:
    public slave_t
{
    public:        
        thread_t(engine_t* engine,
                 const std::string& type,
                 const std::string& args);

        virtual void reap();

    private:
        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;
};

}}}

#endif
