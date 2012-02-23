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

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/slaves.hpp"
#include "cocaine/helpers/tuples.hpp"

namespace cocaine { namespace engine {

class engine_t:
    public boost::noncopyable
{
    public:
#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            const std::string,
            driver::driver_t
        > task_map_t;

#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            unique_id_t::type,
            slave::slave_t
        > pool_map_t;
        
        class job_queue_t:
            public std::deque< boost::shared_ptr<job::job_t> >
        {
            public:
                void push(const_reference job);
        };

        struct idle_slave {
            bool operator()(pool_map_t::pointer slave) const;
        };

        struct specific_slave {
            specific_slave(pool_map_t::pointer target);
            bool operator()(pool_map_t::pointer slave) const;
            pool_map_t::pointer target;
        };

    public:
        engine_t(context_t& context, const std::string& name, const Json::Value& manifest); 
        ~engine_t();

        Json::Value start();
        Json::Value stop();
        Json::Value info() const;

        template<class Selector, class T>
        pool_map_t::iterator unicast(const Selector& selector, const T& command) {
            pool_map_t::iterator it(std::find_if(m_pool.begin(), m_pool.end(), selector));

            if(it != m_pool.end()) {
                m_messages.send_multi(
                    helpers::joint_view(
                        boost::tie(
                            networking::protect(it->second->id())
                        ),
                        command
                    )
                );
            }

            return it;
        }

        void enqueue(job_queue_t::const_reference job, bool overflow = false);

    public:
        context_t& context() {
            return m_context;
        }

        manifest_t& manifest() {
            return m_manifest;
        }

        logging::emitter_t& log() {
            return m_log;
        }

    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void cleanup(ev::timer&, int);

    private:
        bool m_running;
        
        // Runtime context
        context_t& m_context;
        logging::emitter_t m_log;
        manifest_t m_manifest;

        // Application tasks
        task_map_t m_tasks;
        
        // Slave pool
        networking::channel_t m_messages;
        pool_map_t m_pool;
        
        ev::io m_watcher;
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        ev::timer m_gc_timer;

        // Jobs
        job_queue_t m_queue;
};

}}

#endif
