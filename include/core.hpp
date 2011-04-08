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

namespace yappi { namespace core {

// Event loop and networking
class core_t {
    public:
        core_t(const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers);
        virtual ~core_t();

        // The event loop
        void run();
        void signal(int signal) { m_signal = signal; }
    
    public:
        static const char identity[];

    private:
        // Request dispatcher
        void dispatch(const std::string& request);

        // Request handlers
        void start(const std::string& uri, time_t interval);
        void stop(const std::string& key);
        void once(const std::string& uri);

        // Response helpers
        void send(const std::string& response);
        void send(const std::vector<std::string>& response);

    private:
        // Plugin registry
        registry_t m_registry;

        // Engine mappings
        typedef std::map<std::string, std::string> identity_map_t;
        identity_map_t m_identities;

        typedef std::map<std::string, engine::engine_t*> engine_map_t;
        engine_map_t m_engines, m_subscriptions;

        // Networking
        zmq::context_t m_context;
        zmq::socket_t s_sink, s_listener, s_publisher;

        // Loop control
        int m_signal;
    
        // Command regexps
        regex_t r_start, r_stop, r_once;
};

}}

#endif
