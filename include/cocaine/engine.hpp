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
    public helpers::birth_control_t<engine_t>
{
    public:
        engine_t(zmq::context_t& context, boost::shared_ptr<core::core_t> parent,
            const std::string& uri);
        ~engine_t();

        boost::shared_ptr<core::future_t> push(const Json::Value& args);
        boost::shared_ptr<core::future_t> cast(const Json::Value& args);
        boost::shared_ptr<core::future_t> drop(const Json::Value& args);

    private:
        void request(ev::io& w, int revents);
        
    private:
        // Engine ID and a messaging hub endpoint
        helpers::auto_uuid_t m_id;

        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_link;
        ev::io m_link_watcher;

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
    public helpers::birth_control_t<thread_t>
{
    public:
        thread_t(zmq::context_t& context, const helpers::auto_uuid_t& engine_id,
            const std::string& uri);
        ~thread_t();

    public:
        inline std::string id() const;

        inline void enqueue(boost::shared_ptr<core::future_t> future);
        inline boost::shared_ptr<core::future_t> dequeue();
        
#if BOOST_VERSION >= 103500
        void rearm();
#endif

    private:
        void create();
#if BOOST_VERSION >= 103500
        void timeout(ev::timer& w, int revents);
#endif

    private:
        helpers::auto_uuid_t m_id;

        // Thread creation stuff
        zmq::context_t& m_context;
        const helpers::auto_uuid_t& m_engine_id;
        const std::string& m_uri;

        boost::shared_ptr<overseer_t> m_overseer;
        std::auto_ptr<boost::thread> m_thread;

        std::queue< boost::shared_ptr<core::future_t> > m_queue;

#if BOOST_VERSION >= 103500
        ev::timer m_heartbeat;
#endif
};

}}

#endif
