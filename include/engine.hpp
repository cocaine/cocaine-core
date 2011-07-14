#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <boost/ptr_container/ptr_map.hpp>
#include <boost/thread.hpp>

#include "common.hpp"
#include "plugin.hpp"
#include "digest.hpp"

namespace yappi { namespace core {

class future_t;

}}

namespace yappi { namespace engine {

// Thread controller
class engine_t {
    public:
        engine_t(zmq::context_t& context, plugin::source_t* source);
        ~engine_t();

        inline std::string id() const { return m_id.get(); }
        inline std::string hash() const { return m_hash; }

        void schedule(const core::future_t& future, time_t interval);
        void deschedule(const core::future_t& future, time_t interval);
        void once(const core::future_t& future);

        bool stale() const;

    private:
        // Worker thread bootstrap
        void bootstrap();

    private:
        // Engine ID, for interthread pipe identification
        helpers::id_t m_id;

        // Worker thread
        boost::thread* m_thread;

        // Data source
        plugin::source_t* m_source;
        std::string m_hash;
        
        // Messaging
        zmq::context_t& m_context;
        core::json_socket_t m_pipe;
};

// Event fetcher
class fetcher_t {
    public:
        fetcher_t(zmq::context_t& context, plugin::source_t* source, const std::string& key);
        void operator()(ev::timer& timer, int revents);
        
    private:
        // Data source
        plugin::source_t* m_source;
        
        // Messaging
        zmq::socket_t m_uplink;
        
        // Subscription key
        std::string m_key;
};

// Thread manager
class overseer_t {
    public:
        overseer_t(zmq::context_t& context, plugin::source_t* source, const std::string& hash, const helpers::id_t& id);
        void operator()(ev::io& io, int revents);
        void run();
    
    private:
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_io;
        
        // Data source
        plugin::source_t* m_source;
        std::string m_hash;

        // Messaging
        zmq::context_t& m_context;
        core::json_socket_t m_pipe, m_uplink;
        
        // Timers
        typedef boost::ptr_map<time_t, ev::timer> slave_map_t;
        slave_map_t m_slaves;

        // Subscriptions
        typedef std::multimap<time_t, std::string> subscription_map_t;
        subscription_map_t m_subscriptions;
};

}}

#endif
