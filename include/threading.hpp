#ifndef YAPPI_THREADING_HPP
#define YAPPI_THREADING_HPP

#define EV_MINIMAL 0
#include <ev++.h>

#include <boost/thread.hpp>

#include "common.hpp"
#include "plugin.hpp"
#include "networking.hpp"
#include "digest.hpp"

namespace yappi { namespace engine { 

namespace drivers {
    class abstract_t;
}

namespace threading {

// Thread manager
class overseer_t:
    public boost::noncopyable
{
    public:
        overseer_t(helpers::auto_uuid_t id, zmq::context_t& context);
       
        // Thread entry point 
        void run(boost::shared_ptr<plugin::source_t> source);
        
    public:
        // Fetches the new data from the source
        const plugin::dict_t& invoke();
        
        // Bindings for drivers
        inline ev::dynamic_loop& loop() { return m_loop; }
        inline zmq::context_t& context() { return m_context; }

        // Driver termination request
        void reap(const std::string& driver_id);

    private:
        // Event loop callbacks
        void request(ev::io& w, int revents);
        void timeout(ev::timer& w, int revents);
        void cleanup(ev::prepare& w, int revents);

        // Command disptach 
        template<class DriverType>
        void push(const Json::Value& message);
       
        template<class DriverType>
        void drop(const Json::Value& message);
        
        void once(const Json::Value& message);

        void terminate();

        // Suicide request
        void suicide();

        template<class T>
        inline void respond(const Json::Value& future, const T& value) {
            Json::Value response;
            
            response["future"] = future["id"];
            response["engine"] = m_source->uri();
            response["result"] = value;

            m_futures.send_json(response);
        }

    private:
        // Thread ID
        helpers::auto_uuid_t m_id;

        // Messaging
        zmq::context_t& m_context;
        net::json_socket_t m_pipe, m_futures, m_reaper;
        
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
      
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_io;
        ev::timer m_suicide;
        ev::prepare m_cleanup;
        
        // Slaves (Driver ID -> Driver)
        typedef boost::ptr_map<const std::string, drivers::abstract_t> slave_map_t;
        slave_map_t m_slaves;

        // Subscriptions (Driver ID -> Tokens)
        typedef std::multimap<const std::string, std::string> subscription_map_t;
        subscription_map_t m_subscriptions;

        // Hasher (for storage)
        security::digest_t m_digest;

        // Iteration cache
        bool m_cached;
        plugin::dict_t m_cache;
};

// Thread facade
class thread_t:
    public boost::noncopyable,
    public helpers::birth_control_t<thread_t>
{
    public:
        thread_t(helpers::auto_uuid_t id, zmq::context_t& context);
        ~thread_t();

        void run(boost::shared_ptr<plugin::source_t> source);
        inline bool send(const Json::Value& message) { return m_pipe.send_json(message); }

    private:
        helpers::auto_uuid_t m_id;
        net::json_socket_t m_pipe;
        
        std::auto_ptr<overseer_t> m_overseer;
        std::auto_ptr<boost::thread> m_thread;
};

}}}

#endif
