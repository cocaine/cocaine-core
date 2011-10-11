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

// Worker
class thread_t:
    public boost::noncopyable,
    public helpers::birth_control_t<thread_t>,
    public helpers::unique_id_t
{
    public:        
        thread_t(helpers::unique_id_t::type id,
                 helpers::unique_id_t::type engine_id,
                 zmq::context_t& context, 
                 std::string uri);
        ~thread_t();

        void rearm();

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

// Worker pool
class engine_t:
    public boost::noncopyable,
    public helpers::birth_control_t<engine_t>,
    public helpers::unique_id_t
{
    public:
        typedef boost::ptr_map<const std::string, thread_t> thread_map_t;

    public:
        engine_t(zmq::context_t& context, boost::shared_ptr<core::core_t> parent,
                 std::string uri);
        ~engine_t();

        template<class RoutingPolicy, class T>
        boost::shared_ptr<core::future_t> cast(RoutingPolicy route, const T& args) {
            boost::shared_ptr<core::future_t> future(new core::future_t());

            // Try to pick a thread
            thread_map_t::iterator thread(route(m_threads));

            // If the selector has failed to do that...
            if(thread == m_threads.end()) {
                // ...spawn a new one unless we hit the thread limit
                if(m_threads.size() >= config_t::get().engine.maximum_pool_size) {
                    throw std::runtime_error("engine thread limit exceeded");
                }

                // TODO: Preserve the requested thread ID
                helpers::unique_id_t thread_id;

                try {
                    boost::tie(thread, boost::tuples::ignore) = m_threads.insert(thread_id.id(),
                        new thread_t(thread_id.id(), id(), m_context, m_uri));
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

            return future;
        }
        
    private:
        void request(ev::io& w, int revents);
        
    private:
        // Messaging
        zmq::context_t& m_context;
        lines::channel_t m_channel;
        ev::io m_watcher;

        // Parent
        boost::shared_ptr<core::core_t> m_parent;

        // Source URI
        const std::string m_uri;
        
        // Thread management (Thread ID -> Thread)
        thread_map_t m_threads;
};

}}

#endif
