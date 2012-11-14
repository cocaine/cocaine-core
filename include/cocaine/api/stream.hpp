#ifndef COCAINE_STREAM_API_HPP
#define COCAINE_STREAM_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/traits.hpp"

namespace cocaine { namespace api {

struct stream_t {
    virtual
    ~stream_t() {
        // Empty.
    }
    
    virtual
    void
    push(const void * chunk,
         size_t size) = 0;

    template<class T>
    void
    push(const T& object) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);

        io::type_traits<T>::pack(buffer, object);

        push(buffer.data(), buffer.size());
    }

    template<class T>
    friend
    stream_t&
    operator << (stream_t& stream,
                 const T& object)
    {
        stream.push(object);
        return stream;
    }

    virtual
    void
    error(error_code code,
          const std::string& message) = 0;

    virtual
    void
    close() = 0;
};

}}

#endif
