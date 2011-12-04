#ifndef COCAINE_MESSAGES_HPP
#define COCAINE_MESSAGES_HPP

#include <msgpack.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace engine { namespace messages {

enum types {
    invoke = 1,
    terminate,
    chunk,
    choke,
    error,
    suicide,
    heartbeat
};

struct invoke_t {
    invoke_t():
        type(invoke)
    { }

    invoke_t(const std::string& method_):
        type(invoke),
        method(method_)
    { }

    unsigned int type;
    std::string method;

    MSGPACK_DEFINE(type, method);
};

struct terminate_t {
    terminate_t():
        type(terminate)
    { }

    unsigned int type;

    MSGPACK_DEFINE(type);
};

struct chunk_t {
    chunk_t():
        type(chunk)
    { }

    unsigned int type;

    MSGPACK_DEFINE(type);
};

struct choke_t {
    choke_t():
        type(choke)
    { }

    unsigned int type;

    MSGPACK_DEFINE(type);
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

struct suicide_t {
    suicide_t():
        type(suicide)
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

}}}

#endif
