#ifndef COCAINE_CLIENT_HPP
#define COCAINE_CLIENT_HPP

#include <msgpack.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace client {

struct policy_t {
    policy_t():
        urgent(false),
        timeout(0.0f),
        deadline(0.0f)
    { }

    policy_t(bool urgent_, ev::tstamp timeout_, ev::tstamp deadline_):
        urgent(urgent_),
        timeout(timeout_),
        deadline(deadline_)
    { }

    bool urgent;
    ev::tstamp timeout;
    ev::tstamp deadline;

    MSGPACK_DEFINE(urgent, timeout, deadline);
};

enum message_type {
    tag = 1,
    error
};

struct tag_t {
    tag_t():
        type(0)
    { }

    tag_t(const unique_id_t::type& id_, bool completed_ = false):
        type(tag),
        id(id_),
        completed(completed_)
    { }

    unsigned int type;
    unique_id_t::type id;
    bool completed;

    MSGPACK_DEFINE(type, id, completed);
};

enum error_code {
    request_error  = 400,
    server_error   = 500,
    app_error      = 502,
    resource_error = 503,
    timeout_error  = 504
};

struct error_t {
    error_t():
        type(0)
    { }

    error_t(const unique_id_t::type& id_, unsigned int code_, const std::string& message_):
        type(error),
        id(id_),
        code(code_),
        message(message_)
    { }

    unsigned int type;
    unique_id_t::type id;
    unsigned int code;
    std::string message;

    MSGPACK_DEFINE(type, id, code, message);
};
    
}}

#endif
