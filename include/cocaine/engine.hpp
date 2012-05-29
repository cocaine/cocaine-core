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

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/slave.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace engine {

// #if BOOST_VERSION >= 104000
// typedef boost::ptr_unordered_map<
// #else
// typedef boost::ptr_map<
// #endif
//     const std::string,
//     drivers::driver_t
// > driver_map_t;

#if BOOST_VERSION >= 104000
typedef boost::ptr_unordered_map<
#else
typedef boost::ptr_map<
#endif
    slave_t::identifier_type,
    slave_t
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
        bool operator()(const T& slave) const {
            return slave->second->template state_downcast<const State*>();
        }
    };

    struct specific {
        specific(const slave_t& slave):
            target(slave)
        { }

        template<class T>
        bool operator()(const T& slave) const {
            return *slave->second == target;
        }
    
        const slave_t& target;
    };
}

// Engine
// ------

class engine_t:
    public boost::noncopyable
{
    public:
        engine_t(context_t& context,
                 ev::loop_ref& loop,
                 const std::string& name);

        ~engine_t();

        void start();
        void stop();
        
        Json::Value info() /* const */;

        void enqueue(job_queue_t::const_reference,
                     bool overflow = false);

        void process_queue();

        template<class S, class Packed>
        pool_map_t::iterator unicast(const S& selector, Packed& packed) {
            pool_map_t::iterator it(
                std::find_if(
                    m_pool.begin(),
                    m_pool.end(),
                    selector
                )
            );

            if(it != m_pool.end()) {
                call(*it->second, packed);
            }

            return it;
        }

        template<class S, class Packed>
        void multicast(const S& selector, Packed& packed) {
            typedef boost::filter_iterator<S, pool_map_t::iterator> filter;
            
            filter it(selector, m_pool.begin(), m_pool.end()),
                   end(selector, m_pool.end(), m_pool.end());
            
            while(it != end) {
                Packed copy(packed);
                call(*it->second, copy);
                ++it;
            }
        }

    public:
        context_t& context() {
            return m_context;
        }

        const app_t& app() const {
            return m_app;
        }

        ev::loop_ref& loop() {
            return m_loop;
        }

#ifdef HAVE_CGROUPS
        cgroup* group() {
            return m_cgroup;
        }
#endif

    private:
        template<class Packed>
        void call(slave_t& slave, Packed& packed) {
            m_bus.send(
                networking::protect(slave.id()),
                ZMQ_SNDMORE
            );

            m_bus.send_multi(packed);
        }

        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void cleanup(ev::timer&, int);

    private:
        context_t& m_context;

        // Current engine state.
        // XXX: Do it in a better way.
        volatile bool m_running;

        // The application.
        app_t m_app;
        // driver_map_t m_drivers;

        // Job queue.
        job_queue_t m_queue;
        boost::mutex m_queue_mutex;

        // Slave pool.
        pool_map_t m_pool;
        
        // Event loop.
        ev::loop_ref& m_loop;

        ev::io m_watcher;
        ev::idle m_processor;
        ev::timer m_pumper;

        // Garbage collector activation timer.
        ev::timer m_gc_timer;

        // Slave RPC.
        networking::channel_t m_bus;

#ifdef HAVE_CGROUPS
        // Control group to put the slaves into.
        cgroup * m_cgroup;
#endif
};

class threaded_engine_t:
    public boost::noncopyable
{
    public:
        threaded_engine_t(context_t& context,
                          const std::string& name);

    public:
        void start();

        Json::Value info();

        void enqueue(job_queue_t::const_reference job);

    private:
        void bootstrap();

        void do_enqueue(ev::async&, int);
        void do_stop(ev::async&, int);

    private:
        ev::dynamic_loop m_loop;

        ev::async m_async_enqueue,
                  m_async_stop;

        engine_t m_engine;

        boost::shared_ptr<boost::thread> m_thread;
};

}}

#endif
