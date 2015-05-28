#pragma once

#include <string>

namespace cocaine {

class stream_t {
public:
    virtual
    ~stream_t() {}

    virtual
    void
    write(const char* chunk, std::size_t size) = 0;

    virtual
    void
    error(int code, const std::string& reason) = 0;

    virtual
    void
    close() = 0;
};

}
