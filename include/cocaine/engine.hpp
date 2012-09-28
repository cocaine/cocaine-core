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
#include <boost/thread/thread.hpp>

#include <deque>

#ifdef HAVE_CGROUPS
    #include <libcgroup.h>
#endif

#include "cocaine/common.hpp"

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/manifest.hpp"
#include "cocaine/master.hpp"
#include "cocaine/profile.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace engine {

#if BOOST_VERSION >= 104000
typedef boost::ptr_unordered_map<
#else
typedef boost::ptr_map<
#endif
    master_t::identifier_type,
    master_t
> pool_map_t;

class job_queue_t:
    public std::deque<
        boost::shared_ptr<job_t>
    >
{
    public:
        void push(const_reference job);
};

// Engine
// ------

class engine_t:
    public boost::noncopyable
{
    public:
        engine_t(context_t& context,
                 const manifest_t& manifest,
                 const profile_t& profile);

        ~engine_t();

        // Operations.
        void start();
        void stop();

        Json::Value info() const;
        
        // Job scheduling.
        bool enqueue(job_queue_t::const_reference job,
                     mode::value mode = mode::normal);

    public:
        ev::loop_ref& loop() {
            return m_loop;
        }

#ifdef HAVE_CGROUPS
        cgroup* group() {
            return m_cgroup;
        }
#endif

    private:
        // Slave I/O.
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void check(ev::prepare&, int);

        // Garbage collection.
        void cleanup(ev::timer&, int);

        // Forced engine termination.
        void terminate(ev::timer&, int);

        // Asynchronous notification.
        void notify(ev::async&, int);

        // Queue processing.
        void pump();

        // Engine termination.
        void shutdown();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // The app manifest and profile.
        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Engine's state.
        enum {
            running,
            stopping,
            stopped
        } m_state;

        // Engine's state synchronization.
        mutable boost::mutex m_mutex;

        // Engine's thread.
        std::auto_ptr<boost::thread> m_thread;        
        
        // Slave RPC bus.
        std::auto_ptr<io::channel_t> m_bus;
  
        // Event loop.
        ev::dynamic_loop m_loop;

        // Slave I/O watchers.
        ev::io m_watcher;
        ev::idle m_processor;
        ev::prepare m_check;
        
        // Garbage collector activation timer and
        // forced termination timer.
        ev::timer m_gc_timer,
                  m_termination_timer;

        // Async notification watcher.
        ev::async m_notification;

        // Job queue.
        job_queue_t m_queue;

        // Job queue synchronization.
        boost::mutex m_queue_mutex;
        boost::condition_variable m_queue_condition;

        // Slave pool.
        pool_map_t m_pool;
        
#ifdef HAVE_CGROUPS
        // Control group to put the slaves into.
        cgroup * m_cgroup;
#endif
};

}} // namespace cocaine::engine

#endif
