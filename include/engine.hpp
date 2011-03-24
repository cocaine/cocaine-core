#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <string>

#include <zmq.hpp>

#include "common.hpp"
#include "plugin.hpp"

// Message structure used by the engines to
// pass events back to the core
struct event_t {
    event_t(const std::string& key_):
        key(key_),
        dict(NULL) {}

    std::string key;
    dict_t* dict;
};

// Engine workload
struct workload_t {
    workload_t(const std::string& key_, source_t* source_, zmq::context_t& context_, time_t interval_):
        key(key_),
        source(source_),
        context(context_)
    {
        clock_parse(interval_, interval);
        pthread_spin_init(&datalock, PTHREAD_PROCESS_PRIVATE);
        pthread_mutex_init(&sleeplock, NULL);
        pthread_cond_init(&terminate, NULL);
    }

    ~workload_t() {
        pthread_cond_destroy(&terminate);
        pthread_mutex_destroy(&sleeplock);
        pthread_spin_destroy(&datalock);
    }

    std::string key;
    source_t* source;
    zmq::context_t& context;
    timespec interval;
    
    // Flow control
    pthread_spinlock_t datalock;
    pthread_mutex_t sleeplock;
    pthread_cond_t terminate;
};

// Engine
class engine_t {
    public:
        static void* poll(void *arg);

        engine_t(const std::string& key, source_t* source, zmq::context_t& context,
            time_t interval, time_t ttl);
        ~engine_t();

        void subscribe(time_t interval, time_t ttl);
        void unsubscribe() { if(m_refs) m_refs--; }

        // This also checks whether the engine has expired, and
        // if it is, stops it
        bool reapable(const timespec& now);

    private:
        unsigned int m_refs;
        timespec m_timestamp;
        time_t m_ttl;

    private:
        pthread_t m_thread;
        workload_t m_workload;
};

#endif
