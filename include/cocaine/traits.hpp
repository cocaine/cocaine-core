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

#include "cocaine/policy.hpp"

#include "cocaine/helpers/json.hpp"

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

template<>
struct type_traits<engine::policy_t> {
    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer,
         const engine::policy_t& object)
    {
        packer.pack_array(3);
        
        packer << object.urgent;
        packer << object.timeout;
        packer << object.deadline;
    }
    
    static
    void
    unpack(const msgpack::object& packed,
           engine::policy_t& object)
    {
        if(packed.type != msgpack::type::ARRAY || packed.via.array.size != 3) {
            throw msgpack::type_error();
        }

        msgpack::object &urgent = packed.via.array.ptr[0],
                        &timeout = packed.via.array.ptr[1],
                        &deadline = packed.via.array.ptr[2];

        urgent >> object.urgent;
        timeout >> object.timeout;
        deadline >> object.deadline;
    }
};

template<>
struct type_traits<Json::Value> {
    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer,
         const Json::Value& object)
    {
        switch(object.type()) {
            case Json::objectValue: {
                packer.pack_map(object.size());

                Json::Value::Members keys(object.getMemberNames());

                for(Json::Value::Members::const_iterator it = keys.begin();
                    it != keys.end();
                    ++it)
                {
                    packer << *it;
                    pack(packer, object[*it]);
                }

                break;
            }

            case Json::arrayValue:
                packer.pack_array(object.size());

                for(Json::Value::const_iterator it = object.begin();
                    it != object.end();
                    ++it)
                {
                    pack(packer, *it);
                }

                break;

            case Json::booleanValue:
                packer << object.asBool();
                break;

            case Json::stringValue:
                packer << object.asString();
                break;

            case Json::realValue:
                packer << object.asDouble();
                break;

            case Json::intValue:
                packer << object.asLargestInt();
                break;

            case Json::uintValue:
                packer << object.asLargestUInt();
                break;

            case Json::nullValue:
                packer << msgpack::type::nil();
                break;
        }
    }
    
    static
    void
    unpack(const msgpack::object& packed,
           Json::Value& object)
    {
        switch(packed.type) {
            case msgpack::type::MAP: {
                msgpack::object_kv * ptr = packed.via.map.ptr,
                                   * const end = ptr + packed.via.map.size;

                for(; ptr < end; ++ptr) {
                    if(ptr->key.type != msgpack::type::RAW) {
                        // Key is not a string.
                        throw msgpack::type_error();
                    }

                    unpack(
                        ptr->val,
                        object[ptr->key.as<std::string>()]
                    );
                }

                break;
            }

            case msgpack::type::ARRAY: {
                msgpack::object * ptr = packed.via.array.ptr,
                                * const end = ptr + packed.via.array.size;
                
                for(unsigned int index = 0; ptr < end; ++ptr, ++index) {
                    unpack(*ptr, object[index]);
                }

                break;
            }

            case msgpack::type::RAW:
                object = packed.as<std::string>();
                break;

            case msgpack::type::DOUBLE:
                object = packed.as<double>();
                break;

            case msgpack::type::POSITIVE_INTEGER:
                object = static_cast<Json::UInt64>(packed.as<uint64_t>());
                break;

            case msgpack::type::NEGATIVE_INTEGER:
                object = static_cast<Json::Int64>(packed.as<int64_t>());
                break;

            case msgpack::type::BOOLEAN:
                object = packed.as<bool>();
                break;

            case msgpack::type::NIL:
                break;
        }
    }
};

}} // namespace cocaine::io

#endif
