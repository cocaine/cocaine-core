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

#ifndef COCAINE_EVENTS_HPP
#define COCAINE_EVENTS_HPP

#include <boost/statechart/event.hpp>

#include "cocaine/forwards.hpp"
#include <cocaine/networking.hpp>

namespace cocaine { namespace engine { namespace events {

namespace sc = boost::statechart;

// Slave events
struct heartbeat_t:
    public sc::event<heartbeat_t>
{ };

struct terminated_t:
    public sc::event<terminated_t>
{ };

// Job events
struct enqueued_t:
    public sc::event<enqueued_t>
{
    enqueued_t(size_t position_):
        position(position_)
    { }

    const size_t position;
};

struct invoked_t:
    public sc::event<invoked_t>
{
    invoked_t(const boost::shared_ptr<job::job_t>& job_):
        job(job_)
    { }

    const boost::shared_ptr<job::job_t>& job;
};

struct chunk_t:
    public sc::event<chunk_t>
{
    chunk_t(zmq::message_t& message_):
        message(message_)
    { }

    zmq::message_t& message;
};

enum error_code {
    request_error  = 400,
    server_error   = 500,
    app_error      = 502,
    resource_error = 503,
    timeout_error  = 504
};

struct error_t:
    public sc::event<error_t>
{
    error_t(error_code code_, const std::string& message_):
        code(code_),
        message(message_)
    { }
    
    const error_code code;
    const std::string& message;
};

struct choked_t:
    public sc::event<choked_t>
{ };

}}}

#endif
