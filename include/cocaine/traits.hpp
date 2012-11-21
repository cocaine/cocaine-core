/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_TYPE_TRAITS_HPP
#define COCAINE_TYPE_TRAITS_HPP

#include <msgpack.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace io {

template<class T>
struct type_traits {
    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer,
         const T& object)
    {
        packer << object;
    }
    
    static
    void
    unpack(const msgpack::object& packed,
           T& object)
    {
        packed >> object;
    }
};

template<class T>
static inline
std::string
pack(const T& object) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    type_traits<T>::pack(packer, object);

    return { buffer.data(), buffer.size() };
}

template<class T, class It>
static inline
T
unpack(It begin,
       It end)
{
    T result;
    msgpack::unpacked unpacked;

    try { 
        msgpack::unpack(
            &unpacked,
            begin,
            std::distance(begin, end)
        );
       
        type_traits<T>::unpack(unpacked.get(), result);
    } catch(const msgpack::type_error& e) {
        throw cocaine::error_t("corrupted object");
    } catch(const std::bad_cast& e) {
        throw cocaine::error_t("corrupted object - type mismatch");
    }
}

}} // namespace cocaine::io

#endif
