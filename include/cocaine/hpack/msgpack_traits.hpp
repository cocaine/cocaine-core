/*
    Copyright (c) 2011-2015 Anton Matveenko <antmat@yandex-team.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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


#pragma once

#include "cocaine/hpack/header.hpp"

#include <msgpack/pack.hpp>
#include <msgpack/object.hpp>

namespace cocaine { namespace hpack {

struct msgpack_traits {

    // Pack a header from static table with predefined value
    template<class Header, class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer) {
        static_assert(boost::mpl::contains<header_static_table_t::headers_storage, Header>::type::value, "Header is not present in static table");
        packer.pack_fix_uint64(header_static_table_t::idx<Header>());
    }

    // Pack a header from static table but with different value
    template<class Header, class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer, header_table_t& table, header::data_t& header_data) {
        size_t pos = header_static_table_t::idx<Header>();
        if(table[pos].get_value() == header_data) {
            packer.pack_fix_uint64(pos);
            return;
        }
        header_t header(Header::name(), header_data);
        table.push(header);
        // true flag means store header in dynamic_table on receiver side
        packer.pack_true();
        packer.pack_fix_uint64(pos);
        packer.pack_raw(header_data.size);
        packer.pack_raw_body(header_data.blob, header_data.size);
    }

    // Pack any other header
    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer, header_table_t& table, header_t& source) {
        size_t pos = table.find_by_full_match(source);
        if(pos) {
            packer.pack_fix_uint64(pos);
            return;
        }
        packer.pack_array(3);
        pos = table.find_by_name(source);
        // true flag means store header in dynamic_table on receiver side
        packer.pack_true();
        table.push(source);
        if(pos) {
            packer.pack_fix_uint64(pos);
        } else {
            packer.pack_raw(source.get_name().size);
            packer.pack_raw_body(source.get_name().blob, source.get_name().size);
        }
        packer.pack_raw(source.get_value().size);
        packer.pack_raw_body(source.get_value().blob, source.get_value().size);
    }

    static inline
    header_t
    unpack(const msgpack::object& source, header_table_t& table) {

        //If header is fully from the table just fill it and return
        if(source.type == msgpack::type::POSITIVE_INTEGER) {
            if(source.via.u64 >= table.size() || source.via.u64 == 0) {
                throw std::system_error(
                    std::make_error_code(std::errc::invalid_argument),
                    "Invalid index for header table"
                );
            }
            return table[source.via.u64];
        }

        // Encode name to header
        header::data_t header_name;
        if(source.via.array.ptr[1].type == msgpack::type::POSITIVE_INTEGER) {
            header_name = table[source.via.array.ptr[1].via.u64].get_name();
        } else {
            header_name.blob = source.via.array.ptr[1].via.raw.ptr;
            header_name.size = source.via.array.ptr[1].via.raw.size;
        }

        // Encode value to header
        auto value = source.via.array.ptr[2];
        header::data_t header_value;
        header_value.blob = value.via.raw.ptr;
        header_value.size = value.via.raw.size;
        //We don't need to store header in the table
        header_t result(header_name, header_value);
        if(!source.via.array.ptr[0].via.boolean) {
            return result;
        }
        table.push(result);
        return result;
    }

    static inline
    bool
    unpack_vector(const msgpack::object& source, header_table_t& table, std::vector<header_t>& target) {
        target.reserve(source.via.array.size);
        for (size_t i = 0; i < source.via.array.size; i++) {
            msgpack::object& obj = source.via.array.ptr[i];
            if(obj.type == msgpack::type::POSITIVE_INTEGER || (
                   obj.type == msgpack::type::ARRAY &&
                   obj.via.array.size == 3 &&
                   //Either to add header to dynamic table or not
                   obj.via.array.ptr[0].type == msgpack::type::BOOLEAN && (
                        //Either reference to table or raw data
                        obj.via.array.ptr[1].type == msgpack::type::POSITIVE_INTEGER ||
                        obj.via.array.ptr[1].type == msgpack::type::RAW
                   ) && (
                        //Either raw data or a number.
                        obj.via.array.ptr[2].type == msgpack::type::RAW ||
                        obj.via.array.ptr[2].type == msgpack::type::POSITIVE_INTEGER
                   )
               )
            ) {
                try {
                    target.push_back(unpack(obj, table));
                } catch (...) {
                    // Just swallow it. We can not do anything here.
                    return false;
                }
            } else {
                return false;
            }
        }
        return true;
    }
};

}} // namespace cocaine::hpack
