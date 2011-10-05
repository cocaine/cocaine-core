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
        
        // Driver termination request handling
        void reap(const std::string& driver_id);

    public:
        // Driver bindings
        inline std::string id() const { return m_id.get(); }
        inline ev::dynamic_loop& loop() { return m_loop; }
        inline zmq::context_t& context() { return m_context; }
        inline lines::channel_t& downstream() { return m_downstream; }

    private:
        // Event loop callback handling and dispatching
        void request(ev::io& w, int revents);
        void timeout(ev::timer& w, int revents);

        // Thread request handling
        template<class DriverType>
        Json::Value push(const Json::Value& args);
        
        Json::Value drop(const std::string& driver_id);
        Json::Value once();
        
        void terminate();

    private:
        // Thread ID
        helpers::auto_uuid_t m_id;

        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_upstream, m_downstream;
        
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
      
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_request;
        ev::timer m_suicide;
        
        // Driver management (Driver ID -> Driver)
        typedef boost::ptr_map<const std::string, drivers::abstract_driver_t> slave_map_t;
        slave_map_t m_slaves;

        // Storage key generation
        security::digest_t m_digest;
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

        // Thread request forwarding
        void push(core::future_t* future, const std::string& target, const Json::Value& args);
        void drop(core::future_t* future, const std::string& target, const Json::Value& args);

        // Thread tracking request handling
        void track();

    private:
        helpers::auto_uuid_t m_id;
        lines::channel_t m_downstream;
        
        std::auto_ptr<overseer_t> m_overseer;
        std::auto_ptr<boost::thread> m_thread;
};

}}}

#endif
