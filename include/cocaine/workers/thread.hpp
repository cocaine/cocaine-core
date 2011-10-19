#ifndef COCAINE_WORKERS_THREAD_HPP
#define COCAINE_WORKERS_THREAD_HPP

#include <queue>

#include <boost/thread.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine {

// Applcation Engine Worker
class thread_t:
    public boost::noncopyable,
    public helpers::birth_control_t<thread_t>,
    public helpers::unique_id_t
{
    public:        
        thread_t(boost::shared_ptr<engine_t> parent,
                 boost::shared_ptr<overseer_t> overseer);
        ~thread_t();

    public:
        void rearm(float timeout);

        void queue_push(boost::shared_ptr<lines::future_t> future);
        boost::shared_ptr<lines::future_t> queue_pop();
        size_t queue_size() const;
        
    private:
        void timeout(ev::timer& w, int revents);

    private:
        boost::shared_ptr<engine_t> m_parent;
        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;

        typedef std::queue< boost::shared_ptr<lines::future_t> > response_queue_t;
        response_queue_t m_queue;

        ev::timer m_heartbeat;
};

}}

#endif
