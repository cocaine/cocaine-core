/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#ifndef COCAINE_EVENTS_HPP
#define COCAINE_EVENTS_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace engine { namespace events {

struct heartbeat { };

struct terminate { };

struct invoke {
    invoke(const boost::shared_ptr<job_t>& job_,
           engine_t * engine_,
           const boost::weak_ptr<engine::master_t>& master_):
        job(job_),
        engine(engine_),
        master(master_)
    { }

    const boost::shared_ptr<job_t>& job;
    engine_t * engine;
    const boost::weak_ptr<engine::master_t>& master;
};

struct chunk {
    chunk(zmq::message_t& message_):
        message(message_)
    { }

    zmq::message_t& message;
};

struct error {
    error(int code_, const std::string& message_):
        code(code_),
        message(message_)
    { }

    const int code;
    const std::string& message;
};

struct choke { };

}}} // namespace cocaine::engine::events

#endif
