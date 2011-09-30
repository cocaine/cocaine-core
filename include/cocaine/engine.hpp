#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include <queue>

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
        engine_t(zmq::context_t& context, const std::string& target);
        ~engine_t();

        // Thread request forwarding
        void push(core::future_t* future, const Json::Value& args);
        void drop(core::future_t* future, const Json::Value& args);

        // Thread tracking request forwarding
        void track(const std::string& thread_id);
        
        // Thread termination request handling
        void reap(const std::string& thread_id);
        
    private:
        zmq::context_t& m_context;

        // Engine URI
        const std::string m_target;
        
        // Default thread to route all requests to
        const std::string m_default_thread_id;

        // Thread management (Thread ID -> Thread)
        typedef boost::ptr_map<const std::string, threading::thread_t> thread_map_t;
        thread_map_t m_threads;

        // Overflow task queue
        std::queue< std::pair<core::future_t*, Json::Value> > m_pending;
};

}}

#endif
