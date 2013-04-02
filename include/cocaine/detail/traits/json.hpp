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

#ifndef COCAINE_JSON_TYPE_TRAITS_HPP
#define COCAINE_JSON_TYPE_TRAITS_HPP

#include "cocaine/traits.hpp"

#include "cocaine/json.hpp"

namespace cocaine { namespace io {

template<>
struct type_traits<Json::Value> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const Json::Value& source) {
        switch(source.type()) {
            case Json::objectValue: {
                packer.pack_map(source.size());

                Json::Value::Members keys(source.getMemberNames());

                for(Json::Value::Members::const_iterator it = keys.begin();
                    it != keys.end();
                    ++it)
                {
                    packer << *it;
                    pack(packer, source[*it]);
                }

                break;
            }

            case Json::arrayValue:
                packer.pack_array(source.size());

                for(Json::Value::const_iterator it = source.begin();
                    it != source.end();
                    ++it)
                {
                    pack(packer, *it);
                }

                break;

            case Json::booleanValue:
                packer << source.asBool();
                break;

            case Json::stringValue:
                packer << source.asString();
                break;

            case Json::realValue:
                packer << source.asDouble();
                break;

            case Json::intValue:
                packer << source.asLargestInt();
                break;

            case Json::uintValue:
                packer << source.asLargestUInt();
                break;

            case Json::nullValue:
                packer << msgpack::type::nil();
                break;
        }
    }

    static inline
    void
    unpack(const msgpack::object& object, Json::Value& target) {
        switch(object.type) {
            case msgpack::type::MAP: {
                msgpack::object_kv *ptr = object.via.map.ptr,
                                   *const end = ptr + object.via.map.size;

                for(; ptr < end; ++ptr) {
                    if(ptr->key.type != msgpack::type::RAW) {
                        // NOTE: The keys should be strings, as the object
                        // representation of the property maps is still a
                        // JSON object.
                        throw msgpack::type_error();
                    }

                    unpack(
                        ptr->val,
                        target[ptr->key.as<std::string>()]
                    );
                }

                break;
            }

            case msgpack::type::ARRAY: {
                msgpack::object *ptr = object.via.array.ptr,
                                *const end = ptr + object.via.array.size;

                for(unsigned int index = 0; ptr < end; ++ptr, ++index) {
                    unpack(*ptr, target[index]);
                }

                break;
            }

            case msgpack::type::RAW:
                target = object.as<std::string>();
                break;

            case msgpack::type::DOUBLE:
                target = object.as<double>();
                break;

            case msgpack::type::POSITIVE_INTEGER:
                target = static_cast<Json::UInt64>(object.as<uint64_t>());
                break;

            case msgpack::type::NEGATIVE_INTEGER:
                target = static_cast<Json::Int64>(object.as<int64_t>());
                break;

            case msgpack::type::BOOLEAN:
                target = object.as<bool>();
                break;

            case msgpack::type::NIL:
                break;
        }
    }
};

}} // namespace cocaine::io

#endif
