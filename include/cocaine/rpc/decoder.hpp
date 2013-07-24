/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_IO_DECODER_HPP
#define COCAINE_IO_DECODER_HPP

#include "cocaine/rpc/message.hpp"

#include <functional>

namespace cocaine { namespace io {

template<class Stream>
struct decoder {
    COCAINE_DECLARE_NONCOPYABLE(decoder)

    typedef Stream stream_type;

    decoder() {
        // Empty.
    }

   ~decoder() {
        if(m_stream) {
            unbind();
        }
    }

    void
    attach(const std::shared_ptr<stream_type>& stream) {
        m_stream = stream;
    }

    template<class MessageHandler, class ErrorHandler>
    void
    bind(MessageHandler message_handler,
         ErrorHandler error_handler)
    {
        m_handle_message = message_handler;

        using namespace std::placeholders;

        m_stream->bind(
            std::bind(&decoder::on_event, this, _1, _2),
            error_handler
        );
    }

    void
    unbind() {
        m_stream->unbind();
        m_handle_message = nullptr;
    }

public:
    std::shared_ptr<stream_type>
    stream() {
        return m_stream;
    }

private:
    size_t
    on_event(const char* data, size_t size) {
        size_t offset = 0,
               checkpoint = 0,
               bulk = 0;

        msgpack::unpack_return rv;
        msgpack::zone zone;

        do {
            msgpack::object object;

            rv = msgpack::unpack(data, size, &offset, &zone, &object);

            switch(rv) {
            case msgpack::UNPACK_EXTRA_BYTES:
            case msgpack::UNPACK_SUCCESS: {
                checkpoint = offset;

                m_handle_message(message_t(object));

                if(rv == msgpack::UNPACK_SUCCESS) {
                    return size;
                }

                if(++bulk == 256) {
                    return checkpoint;
                }
            } break;

            case msgpack::UNPACK_CONTINUE:
                return checkpoint;

            case msgpack::UNPACK_PARSE_ERROR:
                throw std::system_error(make_error_code(rpc_errc::parse_error));
            }
        } while(true);
    }

private:
    std::function<
        void(const message_t&)
    > m_handle_message;

    // Attachable stream.
    std::shared_ptr<stream_type> m_stream;
};

}}

#endif
