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
#include "engines.hpp"
#include "registry.hpp"
#include "digest.hpp"

namespace yappi { namespace core {

// Event loop and networking
class core_t {
    public:
        core_t(const std::vector<std::string>& ctl_eps, const std::vector<std::string>& pub_eps,
            const std::string& path, int64_t watermark, unsigned int threads, time_t interval);
        virtual ~core_t();

        // The event loop
        virtual void run();
        void signal(int signal) { m_signal = signal; }
    
    public:
        static const char identity[];
        static const char version[];

    protected:
        // Request dispatcher
        void dispatch(const std::string& request);

        // Request handlers
        void loop(const std::string& uri, time_t interval, time_t ttl);
        void unloop(const std::string& key);
        void once(const std::string& uri);

        // Response helpers
        void send(const std::string& response);
        void send(const std::vector<std::string>& response);

    protected:
        // Plugin registry
        registry_t m_registry;

        // Key generator
        helpers::digest_t m_keygen;

        // Engines
        typedef std::map<std::string, engines::loop_t*> engines_t;
        engines_t m_engines;

        // Networking
        zmq::context_t m_context;
        zmq::socket_t s_events, s_ctl, s_pub;

        // Loop control
        int m_signal;
        time_t m_interval;
        timespec m_now;
    
        // Command regexps
        regex_t r_loop, r_unloop, r_once;
};

}}

#endif
