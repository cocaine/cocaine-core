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

#include "cocaine/networking.hpp"

namespace cocaine { namespace engine { namespace rpc {    
    
enum codes {
    heartbeat = 1,
    configure,
    terminate,
    invoke,
    push,
    error,
    release
};

// Generic packer
// --------------

template<codes Code> 
struct packed {
    typedef boost::tuple<int> type;

    packed():
        pack(Code)
    { }

    type& get() {
        return pack;
    }

private:
    type pack;
};

// Specific packers
// ----------------

template<>
struct packed<configure> {
    typedef boost::tuple<int, const config_t&> type;

    packed(const config_t& config):
        pack(configure, config)
    { }

    type& get() {
        return pack;
    }

private:
    type pack;
};

template<>
struct packed<invoke> {
    typedef boost::tuple<int, const std::string&, zmq::message_t&> type;

    packed(const std::string& method, const void * data, size_t size):
        message(size),
        pack(invoke, method, message)
    {
        memcpy(
            message.data(),
            data,
            size
        );
    }

    type& get() {
        return pack;
    }

private:
    zmq::message_t message;
    type pack;
};

template<>
struct packed<push> {
    typedef boost::tuple<int, zmq::message_t&> type;

    packed(const void * data, size_t size):
        message(size),
        pack(push, message)
    {
        memcpy(
            message.data(),
            data,
            size
        );
    }

    packed(zmq::message_t& message_):
        pack(push, message)
    {
        message.move(&message_);
    }

    type& get() {
        return pack;
    }

private:
    zmq::message_t message;
    type pack;
};

template<>
struct packed<error> {
    // NOTE: Not a string reference to allow literal error messages.
    typedef boost::tuple<int, int, std::string> type;

    packed(int code, const std::string& message):
        pack(error, code, message)
    { }

    type& get() {
        return pack;
    }

private:
    type pack;
};
    
}}}

#endif
