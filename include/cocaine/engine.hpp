//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include <deque>

#ifdef HAVE_CGROUPS
    #include <libcgroup.h>
#endif

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/app.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/slaves.hpp"

#include "cocaine/helpers/tuples.hpp"

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
    unique_id_t::type,
    slaves::slave_t
> pool_map_t;

class job_queue_t:
    public std::deque< boost::shared_ptr<job_t> >
{
    public:
        void push(const_reference job);
};

class engine_t:
    public object_t
{
    public:
        engine_t(context_t& ctx, 
                 const std::string& name, 
                 const Json::Value& manifest); 

        ~engine_t();

        Json::Value start();
        Json::Value stop();
        Json::Value info() const;

        void enqueue(job_queue_t::const_reference job, bool overflow = false);

    public:
        inline const app_t& app() const {
            return m_app;
        }

#ifdef HAVE_CGROUPS
        inline cgroup *const group() {
            return m_cgroup;
        }
#endif

    private:
        template<class S, class T>
        pool_map_t::iterator unicast(const S& selector, const T& message) {
            pool_map_t::iterator it(
                std::find_if(
                    m_pool.begin(), 
                    m_pool.end(), 
                    selector
                )
            );

            if(it != m_pool.end()) {
                m_messages.send_multi(
                    helpers::joint_view(
                        boost::make_tuple(
                            networking::protect(it->second->id())
                        ),
                        message
                    )
                );
            }

            return it;
        }

        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void cleanup(ev::timer&, int);

    private:
        // The application.
        bool m_running;
        app_t m_app;
        task_map_t m_tasks;

        // Currently queued jobs.
        job_queue_t m_queue;
        
        // Slave pool.
        networking::channel_t m_messages;
        pool_map_t m_pool;

#ifdef HAVE_CGROUPS
        // Control group to put the slaves into.
        cgroup* m_cgroup;
#endif

        // RPC watchers.        
        ev::io m_watcher;
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        // Garbage collector activation timer.
        ev::timer m_gc_timer;
};

}}

#endif
