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

#ifndef COCAINE_EVENTS_HPP
#define COCAINE_EVENTS_HPP

#include <boost/statechart/event.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace engine { namespace events {

namespace sc = boost::statechart;

struct heartbeat_t:
    public sc::event<heartbeat_t>
{ };

struct terminate_t:
    public sc::event<terminate_t>
{ };

struct enqueue_t:
    public sc::event<enqueue_t>
{
    enqueue_t(size_t position_):
        position(position_)
    { }

    const size_t position;
};

struct invoke_t:
    public sc::event<invoke_t>
{
    invoke_t(job_t * job_):
        job(job_)
    { }

    job_t * job;
};

struct push_t:
    public sc::event<push_t>
{
    push_t(zmq::message_t& message_):
        message(message_)
    { }

    zmq::message_t& message;
};

// struct emit_t:
//     public sc::event<emit_t>
// {
//     emit_t(const std::string& key_, zmq::message_t& message_):
//         key(key_),
//         message(message_)
//     { }

//     const std::string key;
//     zmq::message_t& message;
// };

struct delegate_t:
    public sc::event<delegate_t>
{
    delegate_t(const std::string& target_, zmq::message_t& message_):
        target(target_),
        message(message_)
    { }

    const std::string& target;
    zmq::message_t& message;
};

struct error_t:
    public sc::event<error_t>
{
    error_t(int code_, const std::string& message_):
        code(code_),
        message(message_)
    { }

    const int code;
    const std::string& message;
};

struct release_t:
    public sc::event<release_t>
{ };

}}}

#endif
