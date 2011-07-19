#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <boost/ptr_container/ptr_map.hpp>

#include "common.hpp"
#include "plugin.hpp"
#include "persistance.hpp"

namespace yappi { namespace core {

class future_t;

}}

namespace yappi { namespace engine {

// Thread controller
class engine_t: public boost::noncopyable {
    public:
        engine_t(zmq::context_t& context, plugin::source_t& source,
            persistance::storage_t& storage);
        ~engine_t();

        void push(const core::future_t* future, const Json::Value& args);
        void drop(const core::future_t* future, const Json::Value& args);
        void once(const core::future_t* future);

    private:
        // Worker thread bootstrap
        static void* bootstrap(void* args);

    private:
        // Messaging
        zmq::context_t& m_context;
        core::json_socket_t m_pipe;
        
        // Engine ID, for interthread pipe identification
        helpers::auto_uuid_t m_id;

        // Data source and source hash
        plugin::source_t& m_source;

        // Worker thread
        pthread_t m_thread;
       
        // Task persistance
        persistance::storage_t& m_storage;
};

class fetcher_t;

// Thread manager
class overseer_t: public boost::noncopyable {
    public:
        overseer_t(zmq::context_t& context, const helpers::auto_uuid_t& id,
            plugin::source_t& source, persistance::storage_t& storage);
        
        void operator()(ev::io& io, int revents);
        void operator()(ev::timer& timer, int revents);
        
        void run();

    protected:
        friend class fetcher_t;
        void suicide();

    private:
        void push(const Json::Value& message);
        void drop(const Json::Value& message);
        void once(const Json::Value& message);
        void stop();

        template<class T>
        inline void respond(const Json::Value& future, const T& value) {
            Json::Value response;
            
            response["future"] = future;
            response["engine"] = m_source.uri();
            response["result"] = value;

            m_futures.send(response);
        }

    private:
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_io;
        ev::timer m_stall;
        
        // Messaging
        zmq::context_t& m_context;
        core::json_socket_t m_pipe, m_futures, m_reaper;
        
        // Engine ID, for interthread pipe identification
        helpers::auto_uuid_t m_id;
        
        // Hashing machinery
        helpers::digest_t m_digest;

        // Data source
        plugin::source_t& m_source;
        std::string m_hash;
        
        // Timers
        typedef boost::ptr_map<time_t, ev::timer> slave_map_t;
        slave_map_t m_slaves;

        // Subscriptions
        typedef std::multimap<time_t, std::string> subscription_map_t;
        subscription_map_t m_subscriptions;

        // Task persistance
        persistance::storage_t& m_storage;
};

// Event fetcher
class fetcher_t: public boost::noncopyable {
    public:
        fetcher_t(zmq::context_t& context, overseer_t& overseer,
            plugin::source_t& source, const std::string& key);
        
        void operator()(ev::timer& timer, int revents);
        
    private:
        // Parent
        overseer_t& m_overseer;

        // Data source
        plugin::source_t& m_source;
        
        // Messaging
        core::blob_socket_t m_uplink;
        
        // Subscription key
        std::string m_key;
};

}}

#endif
