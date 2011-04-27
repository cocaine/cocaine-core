#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <boost/ptr_container/ptr_map.hpp>

#include "common.hpp"
#include "plugin.hpp"

namespace yappi { namespace engine {

class engine_t {
    public:
        engine_t(plugin::source_t* source, zmq::context_t& context);
        ~engine_t();

        std::string schedule(const identity_t& identity, time_t interval);
        void deschedule(const identity_t& identity, time_t interval);

    private:
        // Source hash for subscription key construction
        std::string m_hash;

        typedef std::multimap<std::string, std::string> subscription_map_t;
        subscription_map_t m_subscriptions;

        // Threading
        static void* bootstrap(void* arg);
        pthread_t m_thread;

        zmq::socket_t m_socket;
        
        struct task_t {
            task_t(plugin::source_t* source_, zmq::context_t& context_):
                source(source_),
                context(context_) {}

            std::auto_ptr<plugin::source_t> source;
            zmq::context_t& context;
        };

        // Event fetcher
        class fetcher_t {
            public:
                fetcher_t(task_t& task, const std::string& key);
                void operator()(ev::timer& timer, int revents);
                
            private:
                std::string m_key;
                
                task_t& m_task;
                zmq::socket_t m_socket;
        };

        // Thread manager
        class overseer_t {
            public:
                overseer_t(task_t& task);

                void operator()(ev::io& io, int revents);
                void run();
            
            private:
                ev::dynamic_loop m_loop;
                ev::io m_io;

                typedef boost::ptr_map<const std::string, ev::timer> slave_map_t;
                slave_map_t m_slaves;
                
                task_t& m_task;
                zmq::socket_t m_socket;
        };
};

}}

#endif
