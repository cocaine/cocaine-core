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

#ifndef COCAINE_RPC_HPP
#define COCAINE_RPC_HPP

#include "cocaine/io.hpp"

namespace cocaine { namespace engine { namespace rpc {    
    
enum {
    heartbeat = 1,
    terminate,
    invoke,
    chunk,
    error,
    choke
};

// Generic packer
// --------------

template<int Code> 
struct packed:
    public boost::tuple<int>
{
    packed():
        boost::tuple<int>(Code)
    { }
};

// Specific packers
// ----------------

template<>
struct packed<invoke>:
    public boost::tuple<int, const std::string&, zmq::message_t&>
{
    packed(const std::string& method, const void * data, size_t size):
        boost::tuple<int, const std::string&, zmq::message_t&>(invoke, method, message),
        message(size)
    {
        memcpy(
            message.data(),
            data,
            size
        );
    }

private:
    zmq::message_t message;
};

template<>
struct packed<chunk>:
    public boost::tuple<int, zmq::message_t&>
{
    packed(const void * data, size_t size):
        boost::tuple<int, zmq::message_t&>(chunk, message),
        message(size)
    {
        memcpy(
            message.data(),
            data,
            size
        );
    }

    packed(zmq::message_t& message_):
        boost::tuple<int, zmq::message_t&>(chunk, message)
    {
        message.move(&message_);
    }

private:
    zmq::message_t message;
};

template<>
struct packed<error>:
    // NOTE: Not a string reference to allow literal error messages.
    public boost::tuple<int, int, std::string>
{
    packed(int code, const std::string& message):
        boost::tuple<int, int, std::string>(error, code, message)
    { }
};
    
}}}

#endif
