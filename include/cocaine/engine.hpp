#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include <deque>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/job.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/slaves.hpp"

namespace cocaine { namespace engine {

class engine_t:
    public boost::noncopyable
{
    public:
        typedef boost::ptr_unordered_map<
            const std::string,
            driver::driver_t
        > task_map_t;

        typedef boost::ptr_unordered_map<
            unique_id_t::type,
            slave::slave_t
        > pool_map_t;
        
        class job_queue_t:
            public std::deque< boost::shared_ptr<job::job_t> >
        {
            public:
                void push(const boost::shared_ptr<job::job_t>& job);
        };

    public: 
        struct idle_slave {
            bool operator()(pool_map_t::pointer slave) const;
        };

    public:
        engine_t(zmq::context_t& context, const std::string& name); 
        ~engine_t();

        Json::Value start(const Json::Value& manifest);
        Json::Value stop();
        Json::Value info() const;

        template<class Selector, class T>
        pool_map_t::iterator unicast(const Selector& selector, const T& message, zmq::message_t& payload) {
            pool_map_t::iterator it(std::find_if(m_pool.begin(), m_pool.end(), selector));

            if(it != m_pool.end() &&
               !m_messages.send_multi(
                    boost::tie(
                        networking::protect(it->second->id()),
                        message.type,
                        message,
                        payload
                    )
                )) 
            {
                return m_pool.end();
            }

            return it;
        }

        void enqueue(const boost::shared_ptr<job::job_t>& job, bool overflow = false);

        // NOTE: It is intentional that all the publishing drivers do that via
        // one socket, so that one can bind to it and receive all the application
        // publications without chaos and bloodshed.
        void publish(const std::string& key, const Json::Value& object);

    public:
        inline std::string name() const { 
            return m_app_cfg.name; 
        }

        inline zmq::context_t& context() { 
            return m_context; 
        }

    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void cleanup(ev::timer&, int);

    private:
        bool m_running;

        zmq::context_t& m_context;
        boost::shared_ptr<networking::socket_t> m_pubsub;
        
        // Application
        task_map_t m_tasks;
        
        struct {
            std::string name, type, args;
            unsigned int version;
        } m_app_cfg;
        
        // Pool
        pool_map_t m_pool;
        networking::channel_t m_messages;
        
        ev::io m_watcher;
        ev::idle m_processor;
        ev::timer m_gc_timer;

        struct engine_policy {
            std::string backend;
            unsigned int pool_limit;
            unsigned int queue_limit;
            ev::tstamp suicide_timeout;
        } m_policy;

        // Jobs
        job_queue_t m_queue;
};

class publication_t:
    public job::job_t
{
    public:
        publication_t(driver::driver_t* parent);

    public:
        virtual void react(const events::response&);
        virtual void react(const events::error&);
};

}}

#endif
