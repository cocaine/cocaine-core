#ifndef YAPPI_ENGINE_HPP
#define YAPPI_ENGINE_HPP

#include <boost/ptr_container/ptr_map.hpp>
#include <boost/thread.hpp>

#include "common.hpp"
#include "registry.hpp"
#include "persistance.hpp"

namespace yappi { namespace core {
    class future_t;
}}

namespace yappi { namespace engine {

// Thread pool manager
class engine_t:
    public boost::noncopyable,
    public helpers::birth_control_t<engine_t>
{
    public:
        engine_t(zmq::context_t& context, core::registry_t& registry,
            persistance::storage_t& storage, const std::string& target);
        ~engine_t();

        // Thread interoperability
        void push(core::future_t* future, const Json::Value& args);
        void drop(core::future_t* future, const Json::Value& args);
        void reap(const std::string& thread_id);

    private:
        zmq::context_t& m_context;
        core::registry_t& m_registry;
        persistance::storage_t& m_storage;
        const std::string m_target;

        std::string m_default_thread_id;

        class thread_t:
            public helpers::birth_control_t< thread_t, helpers::limited_t<100> >
        {
            public:
                thread_t(zmq::context_t& context, std::auto_ptr<plugin::source_t> source,
                    persistance::storage_t& storage,
                    helpers::auto_uuid_t id = helpers::auto_uuid_t());
                ~thread_t();

                inline std::string id() const { return m_id.get(); }

                inline bool send(const Json::Value& message) {
                    return m_pipe.send(message);
                }

            private:
                void bootstrap();
                
                zmq::context_t& m_context;
                net::json_socket_t m_pipe;
                std::auto_ptr<plugin::source_t> m_source;
                persistance::storage_t& m_storage;
                helpers::auto_uuid_t m_id;
                
                std::auto_ptr<boost::thread> m_thread;
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
                persistance::storage_t& storage, helpers::auto_uuid_t id);
            
            void run();
            
            // Event loop callbacks
            void request(ev::io& w, int revents);
            void timeout(ev::timer& w, int revents);
            void cleanup(ev::prepare& w, int revents);

            // Scheduler bindings
            inline ev::dynamic_loop& binding() { return m_loop; }
            plugin::dict_t fetch();
            
            // Scheduler termination request
            void reap(const std::string& scheduler_id);

        private:
            // Command disptach 
            template<class Scheduler>
            void push(const Json::Value& message);
           
            template<class Scheduler>
            void drop(const Json::Value& message);
            
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
            plugin::dict_t m_cache;
            bool m_cached;
    };
}

}}

#endif
