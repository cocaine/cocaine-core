#ifndef YAPPI_CORE_HPP
#define YAPPI_CORE_HPP

#include <boost/ptr_container/ptr_map.hpp>
#include <boost/function.hpp>

#include "common.hpp"
#include "registry.hpp"
#include "security.hpp"
#include "persistance.hpp"
#include "engine.hpp"

namespace yappi { namespace core {

class future_t;

class core_t: public boost::noncopyable {
    public:
        core_t(const std::string& uuid,
               const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers,
               uint64_t hwm, bool purge);
        ~core_t();

        // Event loop
        void run();
        
    private:
        friend class future_t;

        void seal(const std::string& future_id);

    private:
        // Request dispatching
        void request(ev::io& io, int revents);

        // Built-in commands
        void push(future_t* future, const std::string& target,
            const Json::Value& args);
        void drop(future_t* future, const std::string& target,
            const Json::Value& args);

        // Internal event processing
        void event(ev::io& io, int revents);

        // Future processing
        void future(ev::io& io, int revents);

        // Engine reaper
        void reap(ev::io& io, int revents);

        // Signal processing
        void terminate(ev::sig& sig, int revents);
        void reload(ev::sig& sig, int revents);

        // Task recovery
        void recover();

    private:
        // Plugins
        registry_t m_registry;

        // Security
        security::signing_t m_signer;

        // Task persistance
        persistance::storage_t m_storage;

        // Command dispatching
        typedef boost::function<void(
            future_t*,
            const std::string&,
            const Json::Value&)
        > handler_fn_t;

        typedef std::map<const std::string, handler_fn_t> dispatch_map_t;
        dispatch_map_t m_dispatch;

        // Engine management (URI -> Engine)
        typedef boost::ptr_map<const std::string, engine::engine_t> engine_map_t;
        engine_map_t m_engines;

        // Future management
        typedef boost::ptr_map<const std::string, future_t> future_map_t;
        future_map_t m_futures;

        // Networking
        zmq::context_t m_context;
        net::blob_socket_t s_events, s_requests, s_publisher;
        net::json_socket_t s_futures, s_reaper;
        
        // Event loop
        ev::default_loop m_loop;
        ev::io e_events, e_requests, e_futures, e_reaper;
        ev::sig e_sigint, e_sigterm, e_sigquit, e_sighup;
};

}}

#endif
