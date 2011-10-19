#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/future.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/overseer.hpp"
#include "cocaine/registry.hpp"
#include "cocaine/workers/thread.hpp"

namespace cocaine { namespace engine {

// Application Engine
class engine_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<engine_t>,
    public helpers::birth_control_t<engine_t>,
    public helpers::unique_id_t,
    public lines::responder_t,
    public lines::publisher_t
{
    public:
        typedef boost::ptr_map<const std::string, thread_t> thread_map_t;
    
        struct shortest_queue {
            bool operator()(thread_map_t::reference left, thread_map_t::reference right) {
                return left->second->queue_size() < right->second->queue_size();
            }
        };

    public:
        engine_t(zmq::context_t& context, const std::string& name); 
        ~engine_t();

        Json::Value run(const Json::Value& manifest);
        Json::Value stop();
        Json::Value stats();

        void reap(helpers::unique_id_t::type worker_id);

    public:
        template<class T>
        boost::shared_ptr<lines::future_t> queue(const T& args) {
            boost::shared_ptr<lines::future_t> future(new lines::future_t());

            // Try to pick a thread
            thread_map_t::iterator thread(std::min_element(
                m_threads.begin(),
                m_threads.end(), 
                shortest_queue()));

            // If the selector has failed to do that...
            if(thread == m_threads.end() || thread->second->queue_size() >= m_config.queue_depth) {
                // ...spawn a new one unless we hit the thread limit
                if(m_threads.size() >= m_config.worker_limit) {
                    throw std::runtime_error("engine thread pool limit exceeded");
                }

                try {
                    boost::shared_ptr<plugin::source_t> source(
                        core::registry_t::instance()->create(m_name, m_type, m_args));
                    boost::shared_ptr<overseer_t> overseer(
                        new overseer_t(m_context, id(), source));
                    std::auto_ptr<thread_t> worker(
                        new thread_t(shared_from_this(), overseer));

                    std::string worker_id(worker->id());
                    boost::tie(thread, boost::tuples::ignore) = m_threads.insert(worker_id, worker);
                } catch(const zmq::error_t& e) {
                    if(e.num() == EMFILE) {
                        throw std::runtime_error("core thread limit exceeded");
                    } else {
                        throw;
                    }
                }
            }
            
            thread->second->queue_push(future);

            m_messages.send_multi(helpers::joint_view(
                boost::make_tuple(
                    lines::protect(thread->second->id())),
                args));

            ev::get_default_loop().feed_fd_event(m_messages.fd(), ev::READ);

            return future;
        }

        virtual void respond(const lines::route_t& route, const Json::Value& object);
        virtual void publish(const std::string& key, const Json::Value& object);

    public:
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

        // Application configuration
        const std::string m_name;
        std::string m_type, m_args;
        std::string m_route;
        std::string m_callable;
       
        config_t::engine_config_t m_config;

        // Thread I/O
        lines::channel_t m_messages;
        ev::io m_message_watcher;
        ev::idle m_message_processor;

        // Application I/O
        boost::shared_ptr<lines::socket_t> m_server, m_pubsub;
        boost::shared_ptr<ev::io> m_request_watcher;
        boost::shared_ptr<ev::idle> m_request_processor;

        // Tasks
        typedef boost::ptr_map<const std::string, drivers::abstract_driver_t> task_map_t;
        task_map_t m_tasks;
        
        // History
        typedef std::deque< std::pair<ev::tstamp, Json::Value> > history_t;
        typedef boost::ptr_map<const std::string, history_t> history_map_t;
        history_map_t m_histories;
        
        // Threads
        thread_map_t m_threads;
};

}}

#endif
