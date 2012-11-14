/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <deque>

#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 4)
 #include <cstdatomic>
#else
 #include <atomic>
#endif

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/api/isolate.hpp"

namespace cocaine { namespace engine {

class session_queue_t:
    public std::deque<
        boost::shared_ptr<session_t>
    >
{
    public:
        void
        push(const_reference session);

        // Lockable concept implementation
        
        void
        lock() {
            m_mutex.lock();
        }

        void
        unlock() {
            m_mutex.unlock();
        }

    private:
        boost::mutex m_mutex;
};

class engine_t:
    public boost::noncopyable
{
    enum states: int {
        running,
        broken,
        stopping,
        stopped
    };

    public:
        engine_t(context_t& context,
                 const manifest_t& manifest,
                 const profile_t& profile);

        ~engine_t();

        void
        run();

        // Scheduling
        
        boost::shared_ptr<api::stream_t>
        enqueue(const api::event_t& event,
                const boost::shared_ptr<api::stream_t>& upstream,
                engine::mode mode = engine::mode::normal);

        template<class Event>
        bool
        send(const unique_id_t& uuid,
             const io::message<Event>& message);

    public:
        ev::loop_ref&
        loop() {
            return m_loop;
        }

    private:
        void
        on_bus_event(ev::io&, int);
        
        void
        on_ctl_event(ev::io&, int);

        void
        on_bus_check(ev::prepare&, int);
        
        void
        on_ctl_check(ev::prepare&, int);

        void
        on_notification(ev::async&, int);

        void
        on_cleanup(ev::timer&, int);
        
        void
        on_termination(ev::timer&, int);
        
    private:
        void
        process_bus_events();
        
        void
        process_ctl_events();

        void
        pump();
        
        void
        balance();

        void
        shutdown(states target);
        
        void
        stop();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Engine state

        std::atomic<int> m_state;

        // I/O
        
        typedef io::channel<
            rpc::rpc_plane_tag,
            io::policies::shared
        > rpc_channel_t;

        std::unique_ptr<rpc_channel_t> m_bus;
        
        typedef io::channel<
            control::control_plane_tag,
            io::policies::unique
        > control_channel_t;

        std::unique_ptr<control_channel_t> m_ctl;

        // Event loop
        
        ev::dynamic_loop m_loop;

        ev::io m_bus_watcher,
               m_ctl_watcher;

        ev::prepare m_bus_checker,
                    m_ctl_checker;

        ev::timer m_gc_timer,
                  m_termination_timer;

        ev::async m_notification;

        // Session queue
        
        session_queue_t m_queue;
        boost::condition_variable_any m_condition;

        // Slave pool

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            unique_id_t,
            boost::shared_ptr<slave_t>
        > pool_map_t;
        
        pool_map_t m_pool;

        // NOTE: A strong isolate reference, keeping it here
        // avoids isolate destruction, as the factory stores
        // only weak references to the isolate instances.
        api::category_traits<api::isolate_t>::ptr_type m_isolate;
};

template<class Event>
bool
engine_t::send(const unique_id_t& uuid,
               const io::message<Event>& message)
{
    boost::unique_lock<rpc_channel_t> lock(*m_bus);

    io::scoped_option<
        io::options::send_timeout,
        io::policies::shared
    > option(*m_bus, 0);
    
    return m_bus->send(uuid, ZMQ_SNDMORE) &&
           m_bus->send_message(message);
}

}} // namespace cocaine::engine

#endif
