#ifndef COCAINE_CLIENT_HPP
#define COCAINE_CLIENT_HPP

#include "cocaine/common.hpp"
#include "cocaine/job.hpp"

namespace cocaine { namespace client {

enum types {
    request = 1,
    tag,
    error
};

struct request_t {
    unsigned int type;
    unique_id_t::type id;
    engine::job::policy_t policy;

    MSGPACK_DEFINE(type, id, policy);
};

struct tag_t {
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

struct error_t {
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
