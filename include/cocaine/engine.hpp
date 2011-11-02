#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include "cocaine/backends.hpp"
#include "cocaine/common.hpp"
#include "cocaine/deferred.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace engine {

// Application Engine
class engine_t:
    public boost::noncopyable,
    public lines::publisher_t
{
    public:
        typedef boost::ptr_map<
            const std::string,
            backend_t
        > pool_map_t;
    
        typedef boost::ptr_map<
            const std::string,
            driver_t
        > task_map_t;
       
    public: 
        struct shortest_queue {
            bool operator()(pool_map_t::reference left, pool_map_t::reference right);
        };

        struct pause_task {
            void operator()(task_map_t::reference task);
        };
        
        struct resume_task {
            void operator()(task_map_t::reference task);
        };

    public:
        engine_t(zmq::context_t& context, const std::string& name); 
        ~engine_t();

        Json::Value start(const Json::Value& manifest);
        Json::Value stop();
        Json::Value info() const;

        void reap(unique_id_t::reference worker_id);

    public:
        template<class T>
        void enqueue(boost::shared_ptr<lines::deferred_t> deferred, const T& args) {
            pool_map_t::iterator worker;

            while(true) {
                worker = std::min_element(
                    m_pool.begin(),
                    m_pool.end(), 
                    shortest_queue());

                if(worker == m_pool.end() || 
                    (worker->second->active() &&
                     worker->second->queue().size() > m_pool_cfg.spawn_threshold && 
                     m_pool.size() < m_pool_cfg.pool_limit))
                {
                    try {
                        std::auto_ptr<backend_t> object;

                        if(m_pool_cfg.backend == "thread") {
                            object.reset(new backends::thread_t(this, m_app_cfg.type, m_app_cfg.args));
                        } else if(m_pool_cfg.backend == "process") {
                            object.reset(new backends::process_t(this, m_app_cfg.type, m_app_cfg.args));
                        }

                        std::string worker_id(object->id());
                        boost::tie(worker, boost::tuples::ignore) = m_pool.insert(worker_id, object);
                    } catch(const zmq::error_t& e) {
                        if(e.num() == EMFILE) {
                            throw std::runtime_error("zeromq is overloaded");
                        } else {
                            throw;
                        }
                    }
                } else if(!worker->second->active()) {
                    // NOTE: We pause all the tasks here to avoid races for free workers,
                    // and poll only for pool messages, in case the new worker comes up
                    // or some old worker falls below the threshold.
                    std::for_each(m_tasks.begin(), m_tasks.end(), pause_task());
                    
                    ev::get_default_loop().loop(ev::ONESHOT);
            
                    // NOTE: During the oneshot loop invocation, a termination event might happen,
                    // destroying all the engine structures, so abort the operation in that case.        
                    if(!m_running) {
                        return;
                    }

                    std::for_each(m_tasks.begin(), m_tasks.end(), resume_task());                    
                } else if(worker->second->queue().size() >= m_pool_cfg.queue_limit) {
                    throw std::runtime_error("engine is overloaded");
                } else {
                    break;
                }
            }
            
            m_messages.send_multi(
                helpers::joint_view(
                    boost::make_tuple(
                        lines::protect(worker->second->id())),
                    args));

            worker->second->queue().push(deferred);
        
            // XXX: Damn, ZeroMQ, why are you so strange? 
            ev::get_default_loop().feed_fd_event(m_messages.fd(), ev::READ);
        }

        // NOTE: It is intentional that all the publishing drivers do that via
        // one socket, so that one can bind to it and receive all the application
        // publications without chaos and bloodshed.
        virtual void publish(const std::string& key, const Json::Value& object);

    public:
        std::string name() const { return m_app_cfg.name; }
        zmq::context_t& context() { return m_context; }

    private:
        template<class DriverType>
        void schedule(const std::string& method, const Json::Value& args);

        void message(ev::io& w, int revents);
        void process(ev::idle& w, int revents);

    private:
        bool m_running;

        zmq::context_t& m_context;
        boost::shared_ptr<lines::socket_t> m_pubsub;
        
        // The pool
        config_t::engine_cfg_t m_pool_cfg;

        lines::channel_t m_messages;
        ev::io m_watcher;
        ev::idle m_processor;

        pool_map_t m_pool;
        
        // The application
        struct {
            std::string name, type, args;
        } m_app_cfg;
       
        task_map_t m_tasks;
};

}}

#endif
