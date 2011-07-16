#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <boost/ptr_container/ptr_map.hpp>
#include <boost/thread.hpp>

#include "common.hpp"
#include "plugin.hpp"
#include "persistance.hpp"
#include "digest.hpp"

namespace yappi { namespace core {

class future_t;

}}

namespace yappi { namespace engine {

// Thread controller
class engine_t: public boost::noncopyable {
    public:
        engine_t(zmq::context_t& context, plugin::source_t* source);
        ~engine_t();

        void push(const core::future_t* future, time_t interval);
        void drop(const core::future_t* future, time_t interval);
        void once(const core::future_t* future);

        inline std::string hash() const { return m_hash; }

    private:
        // Worker thread bootstrap
        void bootstrap();

    private:
        // Worker thread
        boost::thread* m_thread;

        // Data source and source hash
        plugin::source_t* m_source;
        std::string m_hash;
        
        // Messaging
        zmq::context_t& m_context;
        core::json_socket_t m_pipe;
        
        // Engine ID, for interthread pipe identification
        helpers::auto_uuid_t m_id;
};

class fetcher_t;

// Thread manager
class overseer_t: public boost::noncopyable {
    public:
        overseer_t(zmq::context_t& context, const helpers::auto_uuid_t& id,
            plugin::source_t* source, const std::string& hash);
        
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
        void respond(const Json::Value& future, const T& value) {
            Json::Value response;
            
            response["future"] = future;
            response["engine"] = m_source->uri();
            response["result"] = value;

            m_futures.send(response);
        }

    private:
        // Event loop
        ev::dynamic_loop m_loop;
        ev::io m_io;
        ev::timer m_stall;
        
        // Data source
        plugin::source_t* m_source;
        std::string m_hash;

        // Messaging
        zmq::context_t& m_context;
        core::json_socket_t m_pipe, m_futures, m_reaper;
        
        // Engine ID, for interthread pipe identification
        helpers::auto_uuid_t m_id;
        
        // Timers
        typedef boost::ptr_map<time_t, ev::timer> slave_map_t;
        slave_map_t m_slaves;

        // Subscriptions
        typedef std::multimap<time_t, std::string> subscription_map_t;
        subscription_map_t m_subscriptions;

        // Task persistance
        helpers::digest_t m_digest;
        persistance::file_storage_t m_storage;
};

// Event fetcher
class fetcher_t: public boost::noncopyable {
    public:
        fetcher_t(zmq::context_t& context, overseer_t& overseer,
            plugin::source_t* source, const std::string& key);
        
        void operator()(ev::timer& timer, int revents);
        
    private:
        // Parent
        overseer_t& m_overseer;

        // Data source
        plugin::source_t* m_source;
        
        // Messaging
        core::blob_socket_t m_uplink;
        
        // Subscription key
        std::string m_key;
};

}}

#endif
