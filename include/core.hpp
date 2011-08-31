#ifndef YAPPI_CORE_HPP
#define YAPPI_CORE_HPP

#include "common.hpp"
#include "registry.hpp"
#include "networking.hpp"
#include "engine.hpp"
#include "persistance.hpp"
#include "security.hpp"

namespace yappi { namespace core {

class future_t;

class core_t:
    public boost::noncopyable
{
    friend class future_t;
    
    public:
        core_t(helpers::auto_uuid_t uuid,
               const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers,
               uint64_t hwm, bool purge);
        ~core_t();

        // Event loop
        void run();
        
    private:
        // Signal processing
        void terminate(ev::sig& sig, int revents);
        void reload(ev::sig& sig, int revents);

        // Request dispatching
        void request(ev::io& io, int revents);

        // Commands
        void dispatch(future_t* future, const Json::Value& root);
        
        void push(future_t* future, const std::string& target, const Json::Value& args);
        void drop(future_t* future, const std::string& target, const Json::Value& args);
        void stat(future_t* future);

        // Response processing
        void seal(const std::string& future_id);

        // Internal event processing
        void event(ev::io& io, int revents);

        // Future processing
        void future(ev::io& io, int revents);

        // Engine reaper
        void reap(ev::io& io, int revents);

        // Task recovery
        void recover();

    private:
        // Plugins
        registry_t m_registry;

        // Security
        security::signing_t m_signer;
        
        // Task persistance
        persistance::storage_t m_storage;

        // Engine management (URI -> Engine)
        typedef boost::ptr_map<const std::string, engine::engine_t> engine_map_t;
        engine_map_t m_engines;

        // Future management
        typedef boost::ptr_map<const std::string, future_t> future_map_t;
        future_map_t m_futures;

        // Networking
        zmq::context_t m_context;
        net::blob_socket_t s_events, s_publisher;
        net::json_socket_t s_requests, s_futures, s_reaper;
        
        // Event loop
        ev::default_loop m_loop;
        ev::io e_events, e_requests, e_futures, e_reaper;
        ev::sig e_sigint, e_sigterm, e_sigquit, e_sighup;
};

}}

#endif
