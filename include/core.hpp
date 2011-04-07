#ifndef YAPPI_CORE_HPP
#define YAPPI_CORE_HPP

#include <string>
#include <vector>
#include <map>

#include <regex.h>

#include <zmq.hpp>

#if ZMQ_VERSION < 20100
    #error ZeroMQ version 2.1.0+ required!
#endif

#include "common.hpp"
#include "engine.hpp"
#include "registry.hpp"
#include "uri.hpp"

namespace yappi { namespace core {

// Event loop and networking
class core_t {
    public:
        core_t(const std::vector<std::string>& listeners, const std::vector<std::string>& publishers,
            const std::string& plugin_path);
        virtual ~core_t();

        // The event loop
        virtual void run();
        void signal(int signal) { m_signal = signal; }
    
    public:
        static const char identity[];

    protected:
        // Request dispatcher
        void dispatch(const std::string& request);

        // Request handlers
        void loop(const helpers::uri_t& uri, time_t interval);
        void unloop(const std::string& key);
        void once(const helpers::uri_t& uri);

        // Response helpers
        void send(const std::string& response);
        void send(const std::vector<std::string>& response);

    protected:
        // Plugin registry
        registry_t m_registry;

        // Engines
        typedef std::map<std::string, engine::engine_t*> engine_map_t;
        engine_map_t m_engines;

        // Networking
        zmq::context_t m_context;
        zmq::socket_t s_sink, s_listener, s_publisher;

        // Loop control
        int m_signal;
    
        // Command regexps
        regex_t r_loop, r_unloop, r_once;
};

}}

#endif
