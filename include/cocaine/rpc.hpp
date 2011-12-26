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

#ifndef COCAINE_MESSAGES_HPP
#define COCAINE_MESSAGES_HPP

#include <msgpack.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace engine { namespace rpc {

enum types {
    invoke = 1,
    terminate,
    heartbeat,
    chunk,
    choke,
    error
};

struct invoke_t {
    invoke_t():
        type(invoke)
    { }

    invoke_t(const std::string& method_, zmq::message_t* request_):
        type(invoke),
        method(method_),
        request(static_cast<const char*>(request_->data()), request_->size())
    { }

    unsigned int type;
    std::string method;
    msgpack::type::raw_ref request;

    MSGPACK_DEFINE(type, method, request);
};

struct terminate_t {
    terminate_t():
        type(terminate)
    { }

    unsigned int type;

    MSGPACK_DEFINE(type);
};

struct heartbeat_t {
    heartbeat_t():
        type(heartbeat)
    { }

    unsigned int type;

    MSGPACK_DEFINE(type);
};

struct chunk_t {
    chunk_t():
        type(chunk)
    { }

    chunk_t(const void* data, size_t size):
        type(chunk),
        response(static_cast<const char*>(data), size)
    { }

    unsigned int type;
    msgpack::type::raw_ref response;

    MSGPACK_DEFINE(type, response);
};

struct error_t {
    error_t():
        type(error)
    { }

    error_t(unsigned int code_, const std::string& message_):
        type(error),
        code(code_),
        message(message_)
    { }

    unsigned int type;
    unsigned int code;
    std::string message;

    MSGPACK_DEFINE(type, code, message);
};

struct choke_t {
    choke_t():
        type(choke)
    { }

    unsigned int type;

    MSGPACK_DEFINE(type);
};

}}}

#endif
