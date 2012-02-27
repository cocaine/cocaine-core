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

#ifndef COCAINE_OVERSEER_HPP
#define COCAINE_OVERSEER_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/networking.hpp"

namespace cocaine { namespace engine {

class invocation_site_t {
    public:
        void push(const void* data, size_t size);

    public:
        const void* request;
        size_t request_size;
};

class plugin_t:
    public object_t
{
    public:
        virtual void initialize(app_t& app) = 0;
        virtual void invoke(invocation_site_t& site) = 0;
};

class unrecoverable_error_t:
    public std::runtime_error
{
    public:
        unrecoverable_error_t(const std::string& what):
            std::runtime_error(what)
        { }
};

class recoverable_error_t:
    public std::runtime_error
{
    public:
        recoverable_error_t(const std::string& what):
            std::runtime_error(what)
        { }
};

class overseer_t:
    public unique_id_t,
    public object_t
{
    public:
        overseer_t(const unique_id_t::type& id,
                   context_t& ctx,
                   app_t& app);

        ~overseer_t();

        // Entry point
        void loop();

    private:
        // Event loop callback handling and dispatching
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void pump(ev::timer&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);
        void terminate();

    private:
        app_t& m_app;

        // Messaging
        networking::channel_t m_messages;

        // Application instance
        std::auto_ptr<plugin_t> m_module;

        // Event loop
        ev::dynamic_loop m_loop;
        
        ev::io m_watcher;
        ev::idle m_processor;
        
        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        ev::timer m_pumper;

        ev::timer m_suicide_timer, m_heartbeat_timer;
};

}}

#endif
