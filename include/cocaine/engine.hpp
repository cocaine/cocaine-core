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
#include <deque>

#ifdef HAVE_CGROUPS
    #include <libcgroup.h>
#endif

#include "cocaine/common.hpp"

#include "cocaine/app.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/object.hpp"
#include "cocaine/slave.hpp"

namespace cocaine { namespace engine {

#if BOOST_VERSION >= 104000
typedef boost::ptr_unordered_map<
#else
typedef boost::ptr_map<
#endif
    const std::string,
    drivers::driver_t
> task_map_t;

#if BOOST_VERSION >= 104000
typedef boost::ptr_unordered_map<
#else
typedef boost::ptr_map<
#endif
    slave_t::identifier_type,
    slave_t
> pool_map_t;

class job_queue_t:
    public std::deque< boost::shared_ptr<job_t> >
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
    public boost::noncopyable,
    public object_t
{
    public:
        engine_t(context_t& ctx, 
                 const std::string& name, 
                 const Json::Value& manifest);

        ~engine_t();

        void start();
        void stop();
        
        Json::Value info() /* const */;

        void enqueue(job_queue_t::const_reference job, bool overflow = false);

        template<class S, class Packed>
        pool_map_t::iterator unicast(const S& selector, Packed& packed) {
            pool_map_t::iterator it(
                std::find_if(
                    m_pool.begin(),
                    m_pool.end(),
                    selector
                )
            );

            if(it != m_pool.end() && call(*it->second, packed)) {
                return it;
            } else {
                return m_pool.end();
            }
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
        const app_t& app() const {
            return m_app;
        }

#ifdef HAVE_CGROUPS
        cgroup * const group() {
            return m_cgroup;
        }
#endif

    private:
        template<class Packed>
        bool call(slave_t& slave, Packed& packed) {
            try {
                m_messages.send(
                    networking::protect(slave.id()),
                    ZMQ_SNDMORE
                );
            } catch(const zmq::error_t& e) {
                m_app.log->error(
                    "slave %d has disconnected unexpectedly", 
                    slave.id().c_str()
                );

                slave.process_event(events::terminate_t());                
            
                return false;
            }

            m_messages.send_multi(packed.get());

            return true;
        }

        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void cleanup(ev::timer&, int);

    private:
        // Current engine state.
        bool m_running;

        // The application.
        const app_t m_app;
        task_map_t m_tasks;

        // Job queue.
        job_queue_t m_queue;
        
        // Slave pool.
        pool_map_t m_pool;
        
        // RPC watchers.
        ev::io m_watcher;
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        // Garbage collector activation timer.
        ev::timer m_gc_timer;

        // Slave RPC.
        networking::channel_t m_messages;

#ifdef HAVE_CGROUPS
        // Control group to put the slaves into.
        cgroup * m_cgroup;
#endif
};

}}

#endif
