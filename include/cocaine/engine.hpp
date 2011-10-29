#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include "cocaine/backends.hpp"
#include "cocaine/common.hpp"
#include "cocaine/deferred.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"

// Driver types
#define AUTO        1   /* do something every n milliseconds */
#define FILESYSTEM  2   /* do something when there's a change on the filesystem */
#define SERVER      3   /* do something when there's a message on the socket */

namespace cocaine { namespace engine {

// Application Engine
class engine_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<engine_t>,
    public birth_control_t<engine_t>,
    public lines::publisher_t
{
    public:
        typedef boost::ptr_map<
            const std::string,
            backend_t
        > pool_t;
    
        typedef std::map<
            const std::string,
            boost::shared_ptr<drivers::driver_t>
        > task_map_t;
       
    public: 
        struct shortest_queue_t {
            bool operator()(pool_t::reference left, pool_t::reference right);
        };

        struct pause_t {
            void operator()(task_map_t::reference task);
        };
        
        struct resume_t {
            void operator()(task_map_t::reference task);
        };

    public:
        engine_t(zmq::context_t& context, const std::string& name); 
        ~engine_t();

        Json::Value start(const Json::Value& manifest);
        Json::Value stop();
        Json::Value info();

        void reap(unique_id_t::reference worker_id);

    public:
        template<class T>
        void queue(boost::shared_ptr<lines::deferred_t> deferred, const T& args) {
            pool_t::iterator worker;

            while(true) {
                worker = std::min_element(
                    m_pool.begin(),
                    m_pool.end(), 
                    shortest_queue_t());

                if(worker == m_pool.end() || 
                    (worker->second->active() &&
                     worker->second->queue().size() > m_pool_cfg.spawn_threshold && 
                     m_pool.size() < m_pool_cfg.pool_limit))
                {
                    try {
                        std::auto_ptr<backend_t> object;

                        if(m_pool_cfg.backend == "thread") {
                            object.reset(new thread_t(shared_from_this(), m_app_cfg.type, m_app_cfg.args));
                        } else if(m_pool_cfg.backend == "process") {
                            object.reset(new process_t(shared_from_this(), m_app_cfg.type, m_app_cfg.args));
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
                    // NOTE: We pause all the tasks here to avoid races for workers,
                    // and wait only for pool messages, in case of the new worker to 
                    // come alive or some old worker fall below the threshold.
                    std::for_each(m_tasks.begin(), m_tasks.end(), pause_t());
                    ev::get_default_loop().loop(ev::ONESHOT);
                    std::for_each(m_tasks.begin(), m_tasks.end(), resume_t());                    
                } else if(worker->second->queue().size() >= m_pool_cfg.queue_limit) {
                    throw std::runtime_error("engine is overloaded");
                } else {
                    break;
                }
            }
            
            m_messages.send_multi(
                helpers::joint_view(
                    boost::make_tuple(
                        lines::protect(worker->second->id()),
                        deferred->id()),
                    args));
            
            worker->second->queue().insert(
                std::make_pair(
                    deferred->id(),
                    deferred));
        
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
        zmq::context_t& m_context;
        
        // Pool configuration
        config_t::engine_cfg_t m_pool_cfg;

        // Pool I/O
        lines::channel_t m_messages;
        ev::io m_watcher;
        ev::idle m_processor;

        // Application configuration
        struct {
            std::string name, type, args;
        } m_app_cfg;
       
        // Tasks
        task_map_t m_tasks;
        
        // Workers
        pool_t m_pool;
        
        // Publishing
        boost::shared_ptr<lines::socket_t> m_pubsub;
};

}}

#endif
