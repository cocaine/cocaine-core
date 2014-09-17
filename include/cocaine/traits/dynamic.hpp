/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_DYNAMIC_SERIALIZATION_TRAITS_HPP
#define COCAINE_DYNAMIC_SERIALIZATION_TRAITS_HPP

#include <cocaine/traits.hpp>

#include <cocaine/dynamic/dynamic.hpp>

namespace cocaine { namespace io {

// Dynamic objects essentially have the same structure as msgpack objects, so this serialization traits
// are pretty much straightforward.

namespace aux {

template<class Stream>
struct pack_dynamic:
    public boost::static_visitor<>
{
    pack_dynamic(msgpack::packer<Stream>& target):
        m_target(target)
    { }

    void
    operator()(const dynamic_t::null_t& COCAINE_UNUSED_(source)) const {
        m_target << msgpack::type::nil();
    }

    void
    operator()(const dynamic_t::bool_t& source) const {
        m_target << source;
    }

    void
    operator()(const dynamic_t::int_t& source) const {
        m_target << source;
    }

    void
    operator()(const dynamic_t::uint_t& source) const {
        m_target << source;
    }

    void
    operator()(const dynamic_t::double_t& source) const {
        m_target << source;
    }

    void
    operator()(const dynamic_t::string_t& source) const {
        m_target << source;
    }

    void
    operator()(const dynamic_t::array_t& source) const {
        m_target.pack_array(source.size());

        for(size_t i = 0; i < source.size(); ++i) {
            source[i].apply(*this);
        }
    }

    void
    operator()(const dynamic_t::object_t& source) const {
        m_target.pack_map(source.size());

        for(auto it = source.begin(); it != source.end(); ++it) {
            m_target << it->first;
            it->second.apply(*this);
        }
    }

private:
    msgpack::packer<Stream>& m_target;
};

} // namespace aux

template<>
struct type_traits<dynamic_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const dynamic_t& source) {
        source.apply(aux::pack_dynamic<Stream>(target));
    }

    static inline
    void
    unpack(const msgpack::object& source, dynamic_t& target) {
        switch(source.type) {
        case msgpack::type::MAP: {
            dynamic_t::object_t container;

            msgpack::object_kv *ptr = source.via.map.ptr,
                               *const end = ptr + source.via.map.size;

            for(; ptr < end; ++ptr) {
                if(ptr->key.type != msgpack::type::RAW) {
                    // NOTE: The keys should be strings.
                    throw msgpack::type_error();
                }

                unpack(ptr->val, container[ptr->key.as<std::string>()]);
            }

            target = std::move(container);
        } break;

        case msgpack::type::ARRAY: {
            dynamic_t::array_t container;
            container.reserve(source.via.array.size);

            msgpack::object *ptr = source.via.array.ptr,
                            *const end = ptr + source.via.array.size;

            for(unsigned int index = 0; ptr < end; ++ptr, ++index) {
                container.push_back(dynamic_t());
                unpack(*ptr, container.back());
            }

            target = std::move(container);
        } break;

        case msgpack::type::RAW: {
            target = source.as<std::string>();
        } break;

        case msgpack::type::DOUBLE: {
            target = source.as<double>();
        } break;

        case msgpack::type::POSITIVE_INTEGER: {
            target = source.as<dynamic_t::uint_t>();
        } break;

        case msgpack::type::NEGATIVE_INTEGER: {
            target = source.as<dynamic_t::int_t>();
        } break;

        case msgpack::type::BOOLEAN: {
            target = source.as<bool>();
        } break;

        case msgpack::type::NIL: {
            target = dynamic_t::null_t();
        }}
    }
};

}} // namespace cocaine::io

#endif
