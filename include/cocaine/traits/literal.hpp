/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_LITERAL_SERIALIZATION_TRAITS_HPP
#define COCAINE_LITERAL_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

namespace cocaine { namespace io {

// This magic specialization allows to pack string literals. It packs only the meaningful bytes,
// trailing zero is dropped. Unpacking is intentionally prohibited as it might force us to silently
// drop characters if the buffer is not long enough.

template<size_t N>
struct type_traits<char[N]> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const char* source) {
        target.pack_raw(N - 1);
        target.pack_raw_body(source, N - 1);
    }
};

// Specialization to pack character arrays without copying to a std::string first.

struct literal_t {
    const char * blob;
    const size_t size;

    // This is needed to mark this struct as implicitly convertible to std::string, although this
    // conversion never takes place, only statically checked in the typelist traits.
    operator std::string() const;
};

template<>
struct type_traits<literal_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const literal_t& source) {
        target.pack_raw(source.size);
        target.pack_raw_body(source.blob, source.size);
    }
};

template<>
struct type_traits<std::string> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const literal_t& source) {
        target.pack_raw(source.size);
        target.pack_raw_body(source.blob, source.size);
    }

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const std::string& source) {
        target << source;
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, std::string& target) {
        unpacked >> target;
    }
};

}} // namespace cocaine::io

#endif
