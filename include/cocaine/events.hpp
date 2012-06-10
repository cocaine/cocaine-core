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

struct heartbeat:
    public sc::event<heartbeat>
{ };

struct terminate:
    public sc::event<terminate>
{ };

struct enqueue:
    public sc::event<enqueue>
{
    enqueue(size_t position_):
        position(position_)
    { }

    const size_t position;
};

struct invoke:
    public sc::event<invoke>
{
    invoke(const boost::shared_ptr<job_t>& job_):
        job(job_)
    { }

    const boost::shared_ptr<job_t>& job;
};

struct chunk:
    public sc::event<chunk>
{
    chunk(zmq::message_t& message_):
        message(message_)
    { }

    zmq::message_t& message;
};

struct error:
    public sc::event<error>
{
    error(int code_, const std::string& message_):
        code(code_),
        message(message_)
    { }

    const int code;
    const std::string& message;
};

struct choke:
    public sc::event<choke>
{ };

}}}

#endif
