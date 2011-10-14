#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include <queue>

#include <boost/thread.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/future.hpp"
#include "cocaine/helpers/tuples.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace engine {

// Applcation Engine Worker
class thread_t:
    public boost::noncopyable,
    public helpers::birth_control_t<thread_t>,
    public helpers::unique_id_t
{
    public:        
        thread_t(helpers::unique_id_t::type engine_id,
                 zmq::context_t& context, 
                 const std::string& uri);
        ~thread_t();

        void rearm(float timeout);

    public:
        inline void queue_push(boost::shared_ptr<core::future_t> future) {
            m_queue.push(future);
        }

        inline boost::shared_ptr<core::future_t> queue_pop() {
            boost::shared_ptr<core::future_t> future(m_queue.front());
            m_queue.pop();

            return future;
        }

        inline size_t queue_size() {
            return m_queue.size();
        }
        
    private:
        void create();
        void timeout(ev::timer& w, int revents);

    private:
        helpers::unique_id_t::type m_engine_id;
        zmq::context_t& m_context;
        std::string m_uri;

        boost::shared_ptr<overseer_t> m_overseer;
        boost::shared_ptr<boost::thread> m_thread;

        typedef std::queue< boost::shared_ptr<core::future_t> > response_queue_t;
        response_queue_t m_queue;

        ev::timer m_heartbeat;
};

// Application Engine
class engine_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<engine_t>,
    public helpers::birth_control_t<engine_t>,
    public helpers::unique_id_t
{
    public:
        typedef boost::ptr_map<const std::string, thread_t> thread_map_t;

    private:
        struct shortest_queue {
            bool operator()(engine_t::thread_map_t::value_type left, engine_t::thread_map_t::value_type right) {
                return left->second->queue_size() < right->second->queue_size();
            }
        };
       
    public:
        engine_t(zmq::context_t& context, const std::string& uri);
        ~engine_t();

        Json::Value run(const Json::Value& args);
        void stop();

        template<class T>
        boost::shared_ptr<core::future_t> queue(const T& args) {
            boost::shared_ptr<core::future_t> future(new core::future_t());

            // Try to pick a thread
            thread_map_t::iterator thread(std::min_element(m_threads.begin(),
                m_threads.end(), shortest_queue()));

            // If the selector has failed to do that...
            if(thread == m_threads.end() || thread->second->queue_size() >= m_config.queue_depth) {
                // ...spawn a new one unless we hit the thread limit
                if(m_threads.size() >= m_config.worker_limit) {
                    throw std::runtime_error("engine thread pool limit exceeded");
                }

                try {
                    std::auto_ptr<thread_t> object(new thread_t(id(), m_context, m_uri));
                    std::string thread_id(object->id());
                    boost::tie(thread, boost::tuples::ignore) = m_threads.insert(thread_id, object);
                } catch(const zmq::error_t& e) {
                    if(e.num() == EMFILE) {
                        throw std::runtime_error("core thread limit exceeded");
                    } else {
                        throw;
                    }
                }
            }
            
            thread->second->queue_push(future);

            m_channel.send_multi(helpers::joint_view(
                boost::make_tuple(
                    lines::protect(thread->second->id())),
                args));

            // Oh, come on...
            ev::get_default_loop().feed_fd_event(m_channel.fd(), EV_READ);
            
            return future;
        }

        inline zmq::context_t& context() { return m_context; }
        
    private:
        template<class DriverType>
        void schedule(const std::string& task, const Json::Value& args);

        void event(ev::io& w, int revents);
        void request(ev::io& w, int revents);
        void publish(const std::string& key, const Json::Value& value);

    private:
        zmq::context_t& m_context;

        // Source URI
        const std::string m_uri;
       
        // Engine configuration 
        config_t::engine_config_t m_config;

        // Thread I/O
        lines::channel_t m_channel;
        ev::io m_channel_watcher;

        // Application I/O
        boost::shared_ptr<lines::socket_t> m_server, m_pubsub;
        boost::shared_ptr<ev::io> m_server_watcher;

        // Tasks
        typedef boost::ptr_map<const std::string, drivers::abstract_driver_t> task_map_t;
        task_map_t m_tasks;
        
        // History (Driver ID -> History List)
        typedef std::deque< std::pair<ev::tstamp, Json::Value> > history_t;
        typedef boost::ptr_map<const std::string, history_t> history_map_t;
        history_map_t m_histories;
                
        // Thread management (Thread ID -> Thread)
        thread_map_t m_threads;
};

}}

#endif
