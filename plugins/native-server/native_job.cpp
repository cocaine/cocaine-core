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

#include "native_job.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;

namespace cocaine { namespace io {

template<>
struct packed<dealer::acknowledgement>:
    public boost::tuple<zmq::message_t&, const std::string&>
{
    typedef boost::tuple<zmq::message_t&, const std::string&> tuple_type;

    packed(const std::string& tag):
        tuple_type(null, tag)
    { }

private:
    zmq::message_t null;
};

template<>
struct packed<dealer::chunk>:
    public boost::tuple<zmq::message_t&, const std::string&, zmq::message_t&>
{
    typedef boost::tuple<zmq::message_t&, const std::string&, zmq::message_t&> tuple_type;

    packed(const std::string& tag, zmq::message_t& message_):
        tuple_type(null, tag, message)
    {
        message.move(&message_);
    }

private:
    zmq::message_t null,
                   message;
};

template<>
struct packed<dealer::error>:
    public boost::tuple<zmq::message_t&, const std::string&, int, const std::string&>
{
    typedef boost::tuple<zmq::message_t&, const std::string&, int, const std::string&> tuple_type;

    packed(const std::string& tag, int code, const std::string& message):
        tuple_type(null, tag, code, message)
    { }

private:
    zmq::message_t null;
};

template<>
struct packed<dealer::choke>:
    public boost::tuple<zmq::message_t&, const std::string&>
{
    typedef boost::tuple<zmq::message_t&, const std::string&> tuple_type;

    packed(const std::string& tag):
        tuple_type(null, tag)
    { }

private:
    zmq::message_t null;
};

}}

native_job_t::native_job_t(const std::string& event, 
                           const blob_t& request,
                           const policy_t& policy,
                           io::channel_t& channel,
                           const io::route_t& route,
                           const std::string& tag):
    job_t(event, request, policy),
    m_channel(channel),
    m_route(route),
    m_tag(tag)
{
    io::packed<dealer::acknowledgement> pack(m_tag);
    m_channel.send(m_route.front(), pack);
}

void native_job_t::react(const events::chunk& event) {
    io::packed<dealer::chunk> pack(m_tag, event.message);
    m_channel.send(m_route.front(), pack);
}

void native_job_t::react(const events::error& event) {
    io::packed<dealer::error> pack(m_tag, event.code, event.message);
    m_channel.send(m_route.front(), pack);
}

void native_job_t::react(const events::choke& event) {
    io::packed<dealer::choke> pack(m_tag);
    m_channel.send(m_route.front(), pack);
}
