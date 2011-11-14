#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include <deque>

#include "cocaine/backends.hpp"
#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/helpers/tuples.hpp"
#include "cocaine/job.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine {

struct resource_error_t:
    public std::runtime_error
{
    resource_error_t(const char* what):
        std::runtime_error(what)
    { }
};

// Application Engine
class engine_t:
    public boost::noncopyable
{
    public:
        typedef boost::ptr_unordered_map<
            const std::string,
            backend_t
        > pool_map_t;
    
        typedef boost::ptr_unordered_map<
            const std::string,
            driver_t
        > task_map_t;

        typedef std::deque<
            boost::shared_ptr<job_t>
        > job_queue_t;
       
    private: 
        struct idle_worker {
            bool operator()(pool_map_t::reference worker);
        };

    public:
        engine_t(zmq::context_t& context, const std::string& name); 
        ~engine_t();

        Json::Value start(const Json::Value& manifest);
        Json::Value stop();
        Json::Value info() const;

        void reap(unique_id_t::reference worker_id);
        void expire(boost::shared_ptr<job_t> job);

        template<class T>
        job_state enqueue(boost::shared_ptr<job_t> job, const T& args) {
            pool_map_t::iterator it(std::find_if(m_pool.begin(), m_pool.end(), idle_worker()));

            if(it != m_pool.end()) {
                m_messages.send_multi(
                    helpers::joint_view(
                        boost::make_tuple(
                            lines::protect(it->second->id())),
                        args));
                
                it->second->arm(job);
                
                return running;
            }
            
            if(m_pool.empty() || m_pool.size() < m_policy.pool_limit) {
                try {
                    std::auto_ptr<backend_t> worker;

                    if(m_policy.backend == "thread") {
                        worker.reset(new backends::thread_t(this, m_app_cfg.type, m_app_cfg.args));
                    } else if(m_policy.backend == "process") {
                        worker.reset(new backends::process_t(this, m_app_cfg.type, m_app_cfg.args));
                    }

                    std::string worker_id(worker->id());
                    m_pool.insert(worker_id, worker);
                } catch(const zmq::error_t& e) {
                    if(e.num() == EMFILE) {
                        throw resource_error_t("unable to spawn more workers");
                    } else {
                        throw;
                    }
                }
            } else if(m_queue.size() > m_policy.queue_limit) {
                throw resource_error_t("queue is full");
            }
       
            if(job->policy().urgent) {
               m_queue.push_front(job);
            } else { 
               m_queue.push_back(job);
            }

            return queued;
        }

        // NOTE: It is intentional that all the publishing drivers do that via
        // one socket, so that one can bind to it and receive all the application
        // publications without chaos and bloodshed.
        virtual void publish(const std::string& key, const Json::Value& object);

        inline std::string name() const { 
            return m_app_cfg.name; 
        }

        inline zmq::context_t& context() { 
            return m_context; 
        }

    private:
        template<class DriverType>
        void schedule(const std::string& method, const Json::Value& args);

        void message(ev::io&, int);
        void process(ev::idle&, int);

    private:
        bool m_running;

        zmq::context_t& m_context;
        boost::shared_ptr<lines::socket_t> m_pubsub;
        
        // The pool
        struct engine_policy {
            std::string backend;
            unsigned int pool_limit;
            unsigned int queue_limit;
            ev::tstamp suicide_timeout;
        } m_policy;

        lines::channel_t m_messages;
        ev::io m_watcher;
        ev::idle m_processor;

        pool_map_t m_pool;
        job_queue_t m_queue;
        
        // The application
        struct {
            std::string name, type, args;
        } m_app_cfg;
       
        task_map_t m_tasks;
};

}}

#endif
