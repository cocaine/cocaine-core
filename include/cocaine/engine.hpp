//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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

#include "cocaine/io.hpp"
#include "cocaine/master.hpp"

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
        void enqueue(job_queue_t::const_reference);

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
        template<class S, class Packed>
        pool_map_t::iterator unicast(const S& selector,
                                     Packed& packed)
        {
            pool_map_t::iterator it(
                std::find_if(
                    m_pool.begin(),
                    m_pool.end(),
                    selector
                )
            );

            if(it != m_pool.end()) {
                send(*it->second, packed);
            }

            return it;
        }

        template<class S, class Packed>
        void multicast(const S& selector,
                       Packed& packed)
        {
            typedef boost::filter_iterator<S, pool_map_t::iterator> filter;
            
            filter it(selector, m_pool.begin(), m_pool.end()),
                   end(selector, m_pool.end(), m_pool.end());
            
            while(it != end) {
                Packed copy(packed);
                send(*it->second, copy);
                ++it;
            }
        }

        template<class Packed>
        void send(master_t& master,
                  Packed& packed)
        {
            m_bus.send(
                io::protect(master.id()),
                ZMQ_SNDMORE
            );

            m_bus.send_multi(packed);
        }

        // Slave I/O.
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);

        // Garbage collection.
        void cleanup(ev::timer&, int);

        // Asynchronous notification.
        void notify(ev::async&, int);

        // Queue processing.
        void react();

        // Engine termination
        void terminate();

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
        ev::timer m_pumper;

        // Garbage collector activation timer.
        ev::timer m_gc_timer;

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
