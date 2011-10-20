#ifndef COCAINE_WORKERS_THREAD_HPP
#define COCAINE_WORKERS_THREAD_HPP

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
        typedef std::map<
            const std::string,
            boost::shared_ptr<lines::future_t>
        > request_queue_t;

    public:        
        thread_t(boost::shared_ptr<engine_t> parent,
                 boost::shared_ptr<overseer_t> overseer);
        ~thread_t();

    public:
        void rearm(float timeout);

        request_queue_t& queue();
        const request_queue_t& queue() const;
        
    private:
        void timeout(ev::timer& w, int revents);

    private:
        boost::shared_ptr<engine_t> m_parent;
        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;

        request_queue_t m_queue;

        ev::timer m_heartbeat;
};

}}

#endif
