#ifndef YAPPI_CORE_HPP
#define YAPPI_CORE_HPP

#include <string>
#include <vector>
#include <map>

#include <regex.h>

#include <zmq.hpp>

#include "common.hpp"
#include "engine.hpp"
#include "digest.hpp"

// Event loop and networking
class core_t {
    public:
        core_t(char* ep_req, char* ep_export, int64_t watermark,
            unsigned int io_threads, time_t interval);
        virtual ~core_t();

        // The event loop
        virtual void run();
        void signal(int signal) { m_signal = signal; }
    
    public:
        static const char identity[];
        static const int version[];

    protected:
        // Request dispatcher
        void dispatch(const std::string& request);

        // Request handlers
        void loop(const std::string& uri, time_t interval, time_t ttl);
        void unloop(const std::string& key);
        void once(const std::string& uri);

        // Responce helpers
        void respond(const std::string& response);
        void publish(const event_t& event);

    protected:
        // Key generator
        digest_t m_keygen;

        // Engines
        typedef std::map<std::string, engine_t*> engines_t;
        engines_t m_engines;

        // Networking
        zmq::context_t m_context;
        zmq::socket_t s_events, s_requests, s_export;

        // Loop control
        int m_signal;
        time_t m_interval;
        timespec m_now;
    
        // Command regexps
        regex_t r_loop, r_unloop, r_once;
};

#endif
