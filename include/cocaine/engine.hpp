#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include <queue>

#include <boost/thread.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace engine {

// Thread pool manager
class engine_t:
    public boost::noncopyable,
    public helpers::birth_control_t<engine_t>,
    public helpers::unique_id_t
{
    public:
        engine_t(zmq::context_t& context, boost::shared_ptr<core::core_t> parent,
                 std::string uri);
        ~engine_t();

        boost::shared_ptr<core::future_t> push(const Json::Value& args);
        boost::shared_ptr<core::future_t> cast(const Json::Value& args);
        boost::shared_ptr<core::future_t> drop(const Json::Value& args);

    private:
        void request(ev::io& w, int revents);
        
    private:
        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_channel;
        ev::io m_watcher;

        // Parent
        boost::shared_ptr<core::core_t> m_parent;

        // Source URI
        const std::string m_uri;
        
        // Thread management (Thread ID -> Thread)
        typedef boost::ptr_map<const std::string, thread_t> thread_map_t;
        thread_map_t m_threads;

        // Overflow task queue
        std::queue< std::pair<boost::shared_ptr<core::future_t>, Json::Value> > m_pending;
};

// Thread interface
class thread_t:
    public boost::noncopyable,
    public helpers::birth_control_t<thread_t>,
    public helpers::unique_id_t
{
    public:
        thread_t(helpers::unique_id_t::type id,
                 zmq::context_t& context, 
                 helpers::unique_id_t::type engine_id,
                 std::string uri);
        ~thread_t();

        void rearm();

    public:
        inline void enqueue(boost::shared_ptr<core::future_t> future);
        inline boost::shared_ptr<core::future_t> dequeue();
        
    private:
        void create();
        void timeout(ev::timer& w, int revents);

    private:
        zmq::context_t& m_context;
        helpers::unique_id_t::type m_engine_id;
        std::string m_uri;

        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;

        std::queue< boost::shared_ptr<core::future_t> > m_queue;

        ev::timer m_heartbeat;
};

}}

#endif
