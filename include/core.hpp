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
        core_t(char* ep_req, char* ep_export, int64_t watermark, unsigned int io_threads);
        virtual ~core_t();

        // The event loop
        virtual void start() = 0;
        void signal(int signal) { m_signal = signal; }
    
    public:
        static const char identity[];
        static const int version[];

    protected:
        // Request dispatcher
        void dispatch(const std::string& request);

        // Request handlers
        void subscribe(const std::string& uri, time_t interval, time_t ttl);
        void unsubscribe(const std::string& uri);

        // A helper to respond with a string
        void respond(const std::string& response);

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
        timespec m_now;
    
        // Command regexps
        regex_t r_subscribe, r_unsubscribe;
};

class poll_core_t: public core_t {
    public:
        poll_core_t(char* ep_req, char* ep_export, int64_t watermark, unsigned int io_threads, time_t interval);
        virtual void start();

    private:
        // Processes and disassemble the events
        void publish(event_t& event);
    
    private:
        // Interval
        time_t m_interval;
};

#endif
