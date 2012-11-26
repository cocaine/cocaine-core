#ifndef COCAINE_STREAM_API_HPP
#define COCAINE_STREAM_API_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace api {

struct stream_t {
    virtual
    ~stream_t() {
        // Empty.
    }
    
    virtual
    void
    push(const char * chunk,
         size_t size) = 0;

    virtual
    void
    error(error_code code,
          const std::string& message) = 0;

    virtual
    void
    close() = 0;
};

struct null_stream_t:
    public stream_t
{
    virtual
    void
    push(const char*, size_t) { }

    virtual
    void
    error(error_code, const std::string&) { }

    virtual
    void
    close() { }
};

}}

#endif
