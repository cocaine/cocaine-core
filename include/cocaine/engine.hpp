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
#include "cocaine/job.hpp"
#include "cocaine/networking.hpp"
#include "cocaine/slaves.hpp"

namespace cocaine { namespace engine {

class engine_t:
    public boost::noncopyable,
    public identifiable_t
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

    public: 
        struct idle_slave {
            bool operator()(pool_map_t::pointer slave) const;
        };

    public:
        engine_t(context_t& context, const std::string& name); 
        ~engine_t();

        Json::Value start(const Json::Value& manifest);
        Json::Value stop();
        Json::Value info() const;

        template<class Selector, class T>
        pool_map_t::iterator unicast(const Selector& selector, const T& message, zmq::message_t* request) {
            pool_map_t::iterator it(std::find_if(m_pool.begin(), m_pool.end(), selector));

            if(it != m_pool.end()) {
                zmq::message_t payload;

                payload.copy(request);

                if(!m_messages.send_multi(
                    boost::tie(
                        networking::protect(it->second->id()),
                        message.type,
                        message,
                        payload
                    ))) 
                {
                    return m_pool.end();
                }
            }

            return it;
        }

        void enqueue(job_queue_t::const_reference job, bool overflow = false);

        // NOTE: This one is a very special method to log and export application
        // publications. Everything intended for various aggregators, log collectors
        // and so on is going through this method.
        void publish(const identifiable_t& source, const Json::Value& object);

    public:
        inline context_t& context() {
            return m_context;
        }

        inline const std::string& name() const { 
            return m_app_cfg.name; 
        }

    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void cleanup(ev::timer&, int);

    private:
        context_t& m_context;
        bool m_running;
        
        boost::shared_ptr<networking::socket_t> m_pubsub;
        
        // Application
        task_map_t m_tasks;
        
        struct {
            std::string name, type, args;
            unsigned int version;
        } m_app_cfg;
        
        // Pool
        networking::channel_t m_messages;
        pool_map_t m_pool;
        
        ev::io m_watcher;
        ev::idle m_processor;

        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        ev::timer m_gc_timer;

        struct engine_policy {
            std::string backend;
            unsigned int pool_limit;
            unsigned int queue_limit;
            ev::tstamp suicide_timeout;
        } m_policy;

        // Jobs
        job_queue_t m_queue;
};

class publication_t:
    public job::job_t
{
    public:
        publication_t(driver::driver_t& parent, const client::policy_t& policy);

    public:
        virtual void react(const events::chunk_t&);
};

}}

#endif
