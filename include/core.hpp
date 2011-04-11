#ifndef YAPPI_CORE_HPP
#define YAPPI_CORE_HPP

#include <regex.h>

#include <zmq.hpp>

#if ZMQ_VERSION < 20100
    #error ZeroMQ version 2.1.0+ required!
#endif

#include <ev++.h>

#include "common.hpp"
#include "engine.hpp"
#include "registry.hpp"

namespace yappi { namespace core {

class core_t {
    public:
        core_t(const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers);
        virtual ~core_t();

        // Event loop
        void run();
        
    public:
        static const char identity[];

    private:
        // Request dispatching and processing
        void dispatch(ev::io& io, int revents);
        void start(const std::string& client, const std::string& uri, time_t interval);
        void stop(const std::string& client, const std::string& key);
        void once(const std::string& client, const std::string& uri);

        // Event processing
        void publish(ev::io& io, int revents);

        // Signal processing
        void terminate(ev::sig& sig, int revents);

        // Messaging helpers
        void send(const std::string& client, const std::string& response);
        void send(const std::string& client, const std::vector<std::string>& response);

    private:
        // Plugins
        registry_t m_registry;

        // Engines
        typedef std::map<std::string, engine::engine_t*> engine_map_t;
        
        // URI -> Engine mapping
        engine_map_t m_engines;

        // Key -> Engine mapping
        engine_map_t m_active;

        // Key -> Clients multimapping
        typedef std::multimap<std::string, std::string> subscription_map_t;
        subscription_map_t m_subscriptions;

        // Networking
        zmq::context_t m_context;
        zmq::socket_t s_sink, s_listener, s_publisher;
        
        // Event loop
        ev::default_loop m_loop;
        ev::io e_sink, e_listener;
        ev::sig e_sigint, e_sigterm, e_sigquit;

        // Command templates
        regex_t r_start, r_stop, r_once;
};

}}

#endif
