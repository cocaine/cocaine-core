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
        thread_t(helpers::unique_id_t::type engine_id,
                 zmq::context_t& context, 
                 const std::string& uri);
        ~thread_t();

        void rearm(float timeout);

    public:
        void queue_push(boost::shared_ptr<lines::future_t> future);
        boost::shared_ptr<lines::future_t> queue_pop();
        size_t queue_size() const;
        
    private:
        void create();
        void timeout(ev::timer& w, int revents);

    private:
        zmq::context_t& m_context;
        
        const helpers::unique_id_t::type m_engine_id;
        const std::string m_uri;

        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;

        typedef std::queue< boost::shared_ptr<lines::future_t> > response_queue_t;
        response_queue_t m_queue;

        ev::timer m_heartbeat;
};

}}

#endif
