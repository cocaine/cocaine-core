#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <string>
#include <set>
#include <map>

#include <zmq.hpp>
#include <ev++.h>

#include "common.hpp"
#include "plugin.hpp"
#include "digest.hpp"

namespace yappi { namespace engine {

class engine_t {
    public:
        engine_t(const std::string& uri, plugin::source_t& source, zmq::context_t& context);
        ~engine_t();

        std::string subscribe(time_t interval);
        void unsubscribe(const std::string& key);

    private:
        // Key management
        std::string m_uri;
        helpers::digest_t m_digest;

        // Thread entry point
        static void* bootstrap(void* arg);
        
        // Thread, in person
        pthread_t m_thread;
        
        // 0MQ thread control socket
        zmq::socket_t m_socket;
        
        // Thread workload structure
        struct task_t {
            task_t(const std::string& uri_, plugin::source_t& source_, zmq::context_t& context_):
                uri(uri_),
                source(source_),
                context(context_) {}

            std::string uri;
            plugin::source_t& source;
            zmq::context_t& context;
        };

        class slave_t {
            public:
                slave_t(ev::dynamic_loop& loop, task_t& task,
                    const std::string& key, time_t interval);
                ~slave_t();

                void operator()(ev::timer& timer, int revents);
                
            private:
                ev::timer m_timer;
                
                plugin::source_t& m_source;
                zmq::socket_t m_socket;
                
                std::string m_key;
        };

        class overseer_t {
            public:
                overseer_t(task_t& task);

                void operator()(ev::io& io, int revents);
                void run();
            
            private:
                ev::dynamic_loop m_loop;
                ev::io m_io;

                task_t& m_task;
                zmq::socket_t m_socket;
    
                typedef std::map<std::string, slave_t*> slave_map_t;
                slave_map_t m_slaves;
        };
};

}}

#endif
