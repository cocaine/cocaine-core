#ifndef YAPPI_CORE_HPP
#define YAPPI_CORE_HPP

#include <boost/function.hpp>
#include <boost/ptr_container/ptr_map.hpp>

#include "common.hpp"
#include "engine.hpp"
#include "registry.hpp"
#include "json/json.h"

namespace yappi { namespace core {

class core_t {
    public:
        core_t(const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers,
               uint64_t hwm, int64_t swap);
        virtual ~core_t();

        // Event loop
        void run();
        
    public:
        static const char identity[];

    private:
        // Request dispatching and processing
        void dispatch(ev::io& io, int revents);
        void reply(const identity_t& identity, const Json::Value& root);

        // Built-in commands
        Json::Value push(const identity_t& identity,
            const std::string& target, const Json::Value& args);
        
        Json::Value drop(const identity_t& identity,
            const std::string& target, const Json::Value& args);
        
        Json::Value once(const identity_t& identity,
            const std::string& target, const Json::Value& args);

        // Event processing
        void publish(ev::io& io, int revents);

        // Signal processing
        void terminate(ev::sig& sig, int revents);

    private:
        // Plugins
        registry_t m_registry;

        // Command dispatching
        typedef boost::function<Json::Value(
            const identity_t&,
            const std::string&,
            const Json::Value&)> handler_fn_t;

        typedef std::map<std::string, handler_fn_t> dispatch_map_t;
        dispatch_map_t m_dispatch;

        // Engine management
        typedef boost::ptr_map<const std::string, engine::engine_t> engine_map_t;
        engine_map_t m_engines;

        typedef std::map<const std::string, engine::engine_t*> weak_engine_map_t;
        weak_engine_map_t m_subscriptions;

        // Networking
        zmq::context_t m_context;
        zmq::socket_t s_sink, s_listener, s_publisher;
        
        // Event loop
        ev::default_loop m_loop;
        ev::io e_sink, e_listener;
        ev::sig e_sigint, e_sigterm, e_sigquit;
};

}}

#endif
