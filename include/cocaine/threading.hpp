#ifndef COCAINE_THREADING_HPP
#define COCAINE_THREADING_HPP

#define EV_MINIMAL 0
#include <ev++.h>

#include <boost/thread.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/security/digest.hpp"

namespace cocaine { namespace engine { namespace threading {

// Thread manager
class overseer_t:
    public boost::noncopyable
{
    public:
        overseer_t(helpers::auto_uuid_t id, zmq::context_t& context);
       
        // Thread entry point 
        void run(boost::shared_ptr<plugin::source_t> source);
        
    public:
        // Driver termination request handling
        void reap(const std::string& driver_id);

    public:
        // Driver bindings
        const Json::Value& invoke();
        inline ev::dynamic_loop& loop() { return m_loop; }
        inline zmq::context_t& context() { return m_context; }

    private:
        // Event loop callback handling
        void command(ev::io& w, int revents);
        void timeout(ev::timer& w, int revents);
        void cleanup(ev::prepare& w, int revents);

        // Thread command disptaching
        template<class DriverType> 
        inline Json::Value dispatch(unsigned int code, const Json::Value& args);

        // Thread command handling
        template<class DriverType> 
        Json::Value push(const Json::Value& args);
        
        template<class DriverType>
        Json::Value drop(const Json::Value& args);
        
        Json::Value once(const Json::Value& args);
        
        void terminate();

        // Suicide request forwarding
        void suicide();

    private:
        // Thread ID
        helpers::auto_uuid_t m_id;

        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_pipe, m_interthread;
        
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
      
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_io;
        ev::timer m_suicide;
        ev::prepare m_cleanup;
        
        // Driver management (Driver ID -> Driver)
        typedef boost::ptr_map<const std::string,
            drivers::abstract_driver_t> slave_map_t;
        slave_map_t m_slaves;

        // Subscription management (Driver ID -> Tokens)
        typedef std::multimap<const std::string, std::string> subscription_map_t;
        subscription_map_t m_subscriptions;

        // Storage key generation
        security::digest_t m_digest;

        // Event caching
        Json::Value m_cache;
        bool m_cached;
};

// Thread interface
class thread_t:
    public boost::noncopyable,
    public helpers::birth_control_t<thread_t>
{
    public:
        thread_t(helpers::auto_uuid_t id, zmq::context_t& context);
        ~thread_t();

        // Thread binding and invoking
        void run(boost::shared_ptr<plugin::source_t> source);

        // Thread command forwarding
        void command(unsigned int code, core::future_t* future, const Json::Value& args);

    private:
        helpers::auto_uuid_t m_id;
        lines::channel_t m_pipe;
        
        std::auto_ptr<overseer_t> m_overseer;
        std::auto_ptr<boost::thread> m_thread;
};

}}}

#endif
