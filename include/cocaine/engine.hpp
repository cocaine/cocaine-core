#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include "cocaine/backends.hpp"
#include "cocaine/common.hpp"
#include "cocaine/deferred.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/overseer.hpp"

// Driver types
#define AUTO        1   /* do something every n milliseconds */
#define CRON        2   /* do something based on a cron-like schedule */
#define MANUAL      3   /* do something when application says */
#define FILESYSTEM  4   /* do something when there's a change on the filesystem */
#define SINK        5   /* do something when there's a message on the socket */

namespace cocaine { namespace engine {

// Application Engine
class engine_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<engine_t>,
    public birth_control_t<engine_t>,
    public lines::responder_t,
    public lines::publisher_t
{
    public:
        typedef boost::ptr_map<
            const std::string,
            backend_t
        > pool_t;
    
        struct shortest_queue {
            bool operator()(pool_t::reference left, pool_t::reference right) {
                return ((left->second->queue().size() < right->second->queue().size()) &&
                         left->second->active());
            }
        };

    public:
        engine_t(zmq::context_t& context, const std::string& name); 
        ~engine_t();

        Json::Value run(const Json::Value& manifest);
        Json::Value stop();
        Json::Value stats();

        void reap(unique_id_t::reference worker_id);

    public:
        template<class T>
        void queue(boost::shared_ptr<lines::deferred_t> deferred, const T& args) {
            pool_t::iterator worker;

            while(true) {
                worker = std::min_element(
                    m_pool.begin(),
                    m_pool.end(), 
                    shortest_queue());

                if(worker == m_pool.end() || 
                    (worker->second->active() &&
                     worker->second->queue().size() > 0 && 
                     m_pool.size() < m_pool_cfg.pool_limit))
                {
                    try {
                        std::auto_ptr<backend_t> object;

                        if(m_pool_cfg.backend == "thread") {
                            object.reset(new thread_t(
                                shared_from_this(),
                                m_app_cfg.type,
                                m_app_cfg.args));
                        } else if(m_pool_cfg.backend == "process") {
                            object.reset(new process_t(
                                shared_from_this(),
                                m_app_cfg.type,
                                m_app_cfg.args));
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
                    // NOTE: We block external communications here to avoid races, i.e.
                    // we go into the inner loop, got another request, found no free workers
                    // and go into a loop one level deeper, and so on.
                    m_request_watcher->stop();
                    m_request_processor->stop();

                    ev::get_default_loop().loop(ev::ONESHOT);
                    
                    m_request_watcher->start(m_messages.fd(), ev::READ);
                    m_request_processor->start();
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

        virtual void respond(const lines::route_t& route, zmq::message_t& chunk);
        virtual void publish(const std::string& key, const Json::Value& object);

    public:
        std::string name() const { return m_app_cfg.name; }
        zmq::context_t& context() { return m_context; }

    private:
        template<class DriverType>
        void schedule(const std::string& task, const Json::Value& args);

        void message(ev::io& w, int revents);
        void process_message(ev::idle& w, int revents);

        void request(ev::io& w, int revents);
        void process_request(ev::idle& w, int revents);

    private:
        zmq::context_t& m_context;
        
        // Pool I/O
        lines::channel_t m_messages;
        ev::io m_message_watcher;
        ev::idle m_message_processor;

        // Application configuration
        config_t::engine_cfg_t m_pool_cfg;

        struct {
            std::string name, type, args;
            std::string server_endpoint, callable;
            std::string pubsub_endpoint;
        } m_app_cfg;
       
        // Application I/O
        boost::shared_ptr<lines::socket_t> m_server, m_pubsub;
        boost::shared_ptr<ev::io> m_request_watcher;
        boost::shared_ptr<ev::idle> m_request_processor;

        // Tasks
        typedef boost::ptr_map<const std::string, drivers::driver_t> task_map_t;
        task_map_t m_tasks;
        
        // History
        typedef std::deque< std::pair<ev::tstamp, Json::Value> > history_t;
        typedef boost::ptr_map<const std::string, history_t> history_map_t;
        history_map_t m_histories;
        
        // Workers
        pool_t m_pool;
};

}}

#endif
