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
#include "cocaine/object.hpp"

#include "cocaine/app.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/slaves.hpp"

#include "cocaine/helpers/tuples.hpp"

namespace cocaine { namespace engine {

class engine_t:
    public object_t
{
    public:
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

        struct idle_slave {
            bool operator()(pool_map_t::pointer slave) const;
        };

        struct specific_slave {
            specific_slave(pool_map_t::pointer target);
            bool operator()(pool_map_t::pointer slave) const;
            pool_map_t::pointer target;
        };

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
                try {
                    m_messages.send_multi(
                        helpers::joint_view(
                            boost::tie(
                                networking::protect(it->second->id())
                            ),
                            message
                        )
                    );
                } catch(const zmq::error_t& e) {
                    // XXX: Fix the error number in 0MQ 3.1
                    if(e.num() == EHOSTDOWN) {
                        log().error(
                            "slave %s has died unexpectedly", 
                            it->second->id().c_str()
                        );

                        it->second->process_event(events::terminate_t());
                        
                        return m_pool.end();
                    } else {
                        throw;
                    }
                }
            }

            return it;
        }

        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void cleanup(ev::timer&, int);

    private:
        app_t m_app;

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
        
        // Active state
        bool m_running;      
};

}}

#endif
