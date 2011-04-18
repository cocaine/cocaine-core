#ifndef YAPPI_CORE_HPP
#define YAPPI_CORE_HPP

#include <boost/function.hpp>

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
        void reply(const std::deque<std::string>& identity, const Json::Value& root);

        // Built-in commands
        void subscribe(const std::deque<std::string>& identity, const std::string& uri, Json::Value& args);
        void unsubscribe(const std::deque<std::string>& identity, const std::string& uri, Json::Value& args);

        // Event processing
        void publish(ev::io& io, int revents);

        // Signal processing
        void terminate(ev::sig& sig, int revents);

    private:
        // Plugins
        registry_t m_registry;

        // Command dispatch table
        typedef boost::function<void(
            const std::deque<std::string>&,
            const std::string&,
            Json::Value&)> command_fn_t;
        std::map<std::string, command_fn_t> m_dispatch;

        // Engines
        typedef std::map<std::string, engine::engine_t*> engine_map_t;
        engine_map_t m_engines;

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
