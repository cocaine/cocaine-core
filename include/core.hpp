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
        virtual void stop() = 0;
    
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
        timespec m_now;
        unsigned int m_watermark;
        bool m_running;
    
        // Command regexps
        regex_t r_subscribe, r_unsubscribe;
};

class timed_core_t: public core_t {
    public:
        timed_core_t(char* ep_req, char* ep_export, int64_t watermark, unsigned int io_threads, time_t interval);

        virtual void start();
        virtual void stop();

    private:
        // Processes and disassemble the events
        void feed(event_t& event);
    
    private:
        // Events pending for publishing
        typedef std::vector<std::string> pending_t;
        pending_t m_pending;

        // Interval
        timespec m_interval;
};

#endif
