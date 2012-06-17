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

#include <boost/iterator/filter_iterator.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <deque>

#ifdef HAVE_CGROUPS
    #include <libcgroup.h>
#endif

#include "cocaine/common.hpp"

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/master.hpp"
#include "cocaine/rpc.hpp"

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

// Selectors
// ---------

namespace select {
    template<class State>
    struct state {
        template<class T>
        bool operator()(const T& master) const {
            return master->second->template state_downcast<const State*>();
        }
    };

    struct specific {
        specific(const master_t& master):
            target(master)
        { }

        template<class T>
        bool operator()(const T& master) const {
            return *master->second == target;
        }
    
        const master_t& target;
    };
}

// Engine
// ------

class engine_t:
    public boost::noncopyable
{
    public:
        engine_t(context_t& context,
                 const manifest_t& manifest);

        ~engine_t();

        // Operations.
        void start();
        void stop();

        Json::Value info() const;
        
        // Job scheduling.
        void enqueue(job_queue_t::const_reference job);

    public:
        const manifest_t& manifest() const {
            return m_manifest;
        }

        ev::dynamic_loop& loop() {
            return m_loop;
        }

#ifdef HAVE_CGROUPS
        cgroup* group() {
            return m_cgroup;
        }
#endif

    private:
        template<class S, int Command>
        pool_map_t::iterator call(const S& selector,
                                  const io::packed<Command>& command)
        {
            pool_map_t::iterator it(
                std::find_if(
                    m_pool.begin(),
                    m_pool.end(),
                    selector
                )
            );

            if(it != m_pool.end()) {
                bool success = m_bus.send(
                    it->second->id(),
                    command,
                    ZMQ_NOBLOCK
                );

                if(!success) {
                    return m_pool.end();
                }
            }

            return it;
        }

        // Slave I/O.
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void check(ev::prepare&, int);
        // void pump(ev::timer&, int);

        // Garbage collection.
        void cleanup(ev::timer&, int);

        // Forced engine termination.
        void terminate(ev::timer&, int);

        // Asynchronous notification.
        void notify(ev::async&, int);

        // Queue processing.
        void pump();

        // Engine shutdown.
        void shutdown();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // Current engine state.
        volatile enum {
            running,
            stopping,
            stopped
        } m_state;

        // The app manifest.
        const manifest_t& m_manifest;

        // Job queue.
        job_queue_t m_queue;

        // Slave pool.
        pool_map_t m_pool;
        
        // Event loop.
        ev::dynamic_loop m_loop;

        // Slave I/O watchers.
        ev::io m_watcher;
        ev::idle m_processor;
        ev::prepare m_check;
        
        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        // ev::timer m_pumper;

        // Garbage collector activation timer and
        // engine termination timer.
        ev::timer m_gc_timer,
                  m_termination_timer;

        // Async notification watcher.
        ev::async m_notification;

        // Slave RPC bus.
        io::channel_t m_bus;
  
        // Threading.
        std::auto_ptr<boost::thread> m_thread;
        mutable boost::mutex m_mutex;

#ifdef HAVE_CGROUPS
        // Control group to put the slaves into.
        cgroup * m_cgroup;
#endif
};

}}

#endif
