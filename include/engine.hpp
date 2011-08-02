#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <boost/ptr_container/ptr_map.hpp>

#include "common.hpp"
#include "registry.hpp"
#include "persistance.hpp"

namespace yappi { namespace core {
    class future_t;
}}

namespace yappi { namespace engine {

// Thread pool manager
class engine_t: public boost::noncopyable {
    public:
        engine_t(zmq::context_t& context, core::registry_t& registry,
            persistance::storage_t& storage, const std::string& target);
        ~engine_t();

        // Thread interoperability
        void start(core::future_t* future, const Json::Value& args);
        void stop(core::future_t* future, const Json::Value& args);
        void reap(const std::string& thread_id);

    private:
        zmq::context_t& m_context;
        core::registry_t& m_registry;
        persistance::storage_t& m_storage;
        const std::string m_target;

        std::string m_default_thread_id;

        class thread_t {
            public:
                thread_t(zmq::context_t& context, std::auto_ptr<plugin::source_t> source,
                    persistance::storage_t& storage);
                ~thread_t();

                inline std::string id() const { return m_uuid.get(); }

                inline void send(const Json::Value& message) {
                    m_pipe.send(message);
                }

            private:
                static void* bootstrap(void* args);
                
                zmq::context_t& m_context;
                net::json_socket_t m_pipe;
                std::auto_ptr<plugin::source_t> m_source;
                persistance::storage_t& m_storage;
                
                helpers::auto_uuid_t m_uuid;
                pthread_t m_thread;
        };

        // Thread ID -> Thread
        typedef boost::ptr_map<const std::string, thread_t> thread_map_t;
        thread_map_t m_threads;
};

namespace {
    class scheduler_base_t;

    // Thread manager
    class overseer_t: public boost::noncopyable {
        public:
            overseer_t(zmq::context_t& context, plugin::source_t& source,
                persistance::storage_t& storage, const helpers::auto_uuid_t& uuid);
            
            void run();
            
            // Event loop callbacks
            void operator()(ev::io& w, int revents);
            void operator()(ev::timer& w, int revents);
            void operator()(ev::prepare& w, int revents);

            // Scheduler bindings
            inline ev::dynamic_loop& binding() { return m_loop; }
            plugin::source_t::dict_t fetch();
            
            // Scheduler termination request
            void reap(const std::string& scheduler_id);

        private:
            // Command disptach 
            template<class Scheduler>
            void start(const Json::Value& message);
           
            template<class Scheduler>
            void stop(const Json::Value& message);
            
            void once(const Json::Value& message);

            void terminate();

            // Suicide request
            void suicide();

            template<class T>
            inline void respond(const Json::Value& future, const T& value) {
                Json::Value response;
                
                response["future"] = future["id"];
                response["engine"] = m_source.uri();
                response["result"] = value;

                m_futures.send(response);
            }

        private:
            // Messaging
            zmq::context_t& m_context;
            net::json_socket_t m_pipe, m_futures, m_reaper;
            
            // Data source
            plugin::source_t& m_source;
            
            // Task persistance
            persistance::storage_t& m_storage;
          
            // Thread ID
            helpers::auto_uuid_t m_id;

            // Event loop
            ev::dynamic_loop m_loop;
            ev::io m_io;
            ev::timer m_suicide;
            ev::prepare m_cleanup;
            
            // Slaves (Scheduler ID -> Scheduler)
            typedef boost::ptr_map<const std::string, scheduler_base_t> slave_map_t;
            slave_map_t m_slaves;

            // Subscriptions (Scheduler ID -> Tokens)
            typedef std::multimap<const std::string, std::string> subscription_map_t;
            subscription_map_t m_subscriptions;

            // Hasher (for storage)
            helpers::digest_t m_digest;

            // Iteration cache
            plugin::source_t::dict_t m_cache;
            bool m_cached;
    };
}

}}

#endif
