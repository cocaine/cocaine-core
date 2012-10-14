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

#if defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 4) && !defined(__clang__)
    #include <cstdatomic>
#else
    #include <atomic>
#endif

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/io.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/isolate.hpp"

namespace cocaine { namespace engine {

#if BOOST_VERSION >= 103600
typedef boost::unordered_map<
#else
typedef std::map<
#endif
    unique_id_t,
    boost::shared_ptr<master_t>
> pool_map_t;

class job_queue_t:
    public std::deque<
        boost::shared_ptr<job_t>
    >
{
    public:
        void push(const_reference job);
};

class engine_t:
    public boost::noncopyable
{
    public:
        engine_t(context_t& context,
                 const manifest_t& manifest,
                 const profile_t& profile);

        ~engine_t();

        void
        run();

        // Job scheduling
        
        bool
        enqueue(job_queue_t::const_reference job,
                mode::value mode = mode::normal);

        template<class T>
        bool
        send(const unique_id_t& uuid,
             const T& message);

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
        shutdown();
        
        void
        stop();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        struct state {
            enum value: int {
                running,
                broken,
                stopping,
                stopped
            };
        };

        std::atomic<int> m_state;

        // I/O
        
        std::unique_ptr<io::channel_t> m_bus,
                                       m_ctl;

        boost::mutex m_bus_mutex;

        // Event loop
        
        ev::dynamic_loop m_loop;

        ev::io m_bus_watcher,
               m_ctl_watcher;

        ev::prepare m_bus_checker,
                    m_ctl_checker;

        ev::timer m_gc_timer,
                  m_termination_timer;

        ev::async m_notification;

        // Job queue
        
        job_queue_t m_queue;
        boost::mutex m_queue_mutex;
        boost::condition_variable m_queue_condition;

        // Slave pool
        
        pool_map_t m_pool;

        // NOTE: Engine isolate reference, keeping it here
        // avoids isolate destruction, as the factory stores
        // only weak references to the isolate instances.
        api::category_traits<api::isolate_t>::ptr_type m_isolate;
};

template<class T>
bool
engine_t::send(const unique_id_t& uuid,
               const T& message)
{
    boost::unique_lock<boost::mutex> lock(m_bus_mutex);

    io::scoped_option<
        io::options::send_timeout
    > option(*m_bus, 0);
    
    return m_bus->send(uuid, ZMQ_SNDMORE) &&
           m_bus->send_message(message);
}

}} // namespace cocaine::engine

#endif
