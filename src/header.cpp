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

#include "cocaine/hpack/header.h"
#include "cocaine/hpack/header.hpp"
#include "cocaine/hpack/static_table.hpp"

#include <cassert>

namespace cocaine { namespace hpack {

size_t
http2_integer_size(size_t sz, size_t bit_offset) {
    if(bit_offset == 0 || bit_offset > 7) {
        throw std::system_error(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid bit_offset for http2_integer_size"
        );
    }
    // See packing here https://httpwg.github.io/specs/rfc7541.html#integer.representation
    // if integer fits to 8 - bit_offset bits
    if(sz < static_cast<size_t>(1 << (8 - bit_offset))) {
        return 1;
    }
    // One byte is first and we start to write in second one
    size_t ret = 2;
    sz -= (1 << (8 - bit_offset));
    while(sz > 127) {
        sz = sz >> 7;
        ret++;
    }
    return ret;
}

// Note dest should be at least 10 bytes long
// See packing here https://httpwg.github.io/specs/rfc7541.html#integer.representation
size_t
http2_integer_encode(unsigned char* dest, uint64_t source, size_t bit_offset, char prefix) {
    if(bit_offset == 0 || bit_offset > 7) {
        throw std::system_error(
            std::make_error_code(std::errc::invalid_argument),
            "Invalid bit_offset for http2_integer_encode"
        );
    }
    dest[0] = prefix;
    unsigned char mask = 255;

    // static_cast is used to remove warning abount conversion
    // as far as all types in bit operations are widened to int.
    mask = static_cast<unsigned char>(~(mask >> bit_offset));
    dest[0] = static_cast<unsigned char>(dest[0] & mask);
    size_t first_byte_cap = 1 << (8-bit_offset);
    if(source < first_byte_cap) {
        dest[0] += static_cast<unsigned char>(source);
        return 1;
    }
    dest[0] += first_byte_cap;
    source -= first_byte_cap;
    // One byte is first and we start to write in second one
    size_t ret = 2;
    while(source > 127) {
        dest[ret-1] = 128 + source % 128;
        source/=128;
        ret++;
    }
    dest[ret-1] = source;
    return ret;
}

struct init_header_t {
    init_header_t(header_static_table_t::storage_t& _data) :
        data(_data)
    {}
    template<class Header>
    void
    operator()(Header) {
        data[boost::mpl::find<header_static_table_t::headers_storage, Header>::type::pos::value].name = Header::name();
        data[boost::mpl::find<header_static_table_t::headers_storage, Header>::type::pos::value].value = Header::value();
    }
    header_static_table_t::storage_t& data;
};

static
header_static_table_t::storage_t
init_data() {
    header_static_table_t::storage_t data;
    init_header_t init(data);
    boost::mpl::for_each<header_static_table_t::headers_storage>(init);
    return data;
}

namespace header {

bool data_t::operator==(const data_t& other) const {
    return size == other.size && (std::memcmp(blob, other.blob, size) == 0);
}

} // namespace header

header_t::zone_t::zone_t(header_t& header) {
    rebind_header(header);
}

header_t::zone_t::zone_t(std::vector<header_t>& headers) {
    rebind_headers(headers);
}

void
header_t::zone_t::reserve(size_t size) {
    storage.reserve(std::max(size, storage.size()));
}

void
header_t::zone_t::rebind_header(header_t& header) {
    size_t cur_size = storage.size();
    storage.resize(cur_size + header.get_name().size + header.get_value().size);
    memcpy(storage.data()+cur_size, header.get_name().blob, header.get_name().size);
    header.name.blob = storage.data()+cur_size;
    cur_size += header.get_name().size;
    memcpy(storage.data()+cur_size, header.get_value().blob, header.get_value().size);
    header.value.blob = storage.data()+cur_size;
}

void
header_t::zone_t::rebind_headers(std::vector<header_t>& headers) {
    auto zone_size = storage.size();

    for(const auto& h: headers) {
        zone_size += h.get_name().size;
        zone_size += h.get_value().size;
    }

    reserve(zone_size);
    for(auto& header: headers) {
        rebind_header(header);
    }
}

bool
header_t::operator==(const header_t& other) const {
    return name == other.name && value == other.value;
}

bool
header_t::name_equal(const header_t& other) const {
    return name == other.name;
}

header_t::header_t(const ch_header& c_header) :
    name({c_header.name.blob, c_header.name.size}),
    value({c_header.value.blob, c_header.value.size})
{}

header_t::header_t(const header::data_t& _name, const header::data_t& _value) noexcept :
    name(_name),
    value(_value)
{}

header_t::header_t(const char* _name, size_t name_sz, const char* _value, size_t value_sz) noexcept :
    name({_name, name_sz}),
    value({_value, value_sz})
{}

header::data_t
header_t::get_name() const {
    return name;
}

header::data_t
header_t::get_value() const {
    return value;
}

size_t
header_t::http2_size() const {
    // 1 refer to string literals which has size with 1-bit padding.
    // See https://tools.ietf.org/html/draft-ietf-httpbis-header-compression-12#section-5.2
    return name.size + http2_integer_size(name.size, 1) + value.size + http2_integer_size(value.size, 1) + header_table_t::http2_header_overhead;
}

header_storage_t::header_storage_t(std::vector<header_t> headers) :
    headers(std::move(headers))
{
    zone.rebind_headers(headers);
}

const std::vector<header_t>&
header_storage_t::get_headers() const noexcept {
    return headers;
}

void
header_storage_t::push_back(const header_t& header) {
    headers.push_back(header);
    zone.rebind_header(headers.back());
}

const header_static_table_t::storage_t&
header_static_table_t::get_headers() {
    static storage_t storage = init_data();
    return storage;
}

header_table_t::header_table_t() :
    header_lower_bound(0),
    header_upper_bound(0),
    data_lower_bound(0),
    data_lower_bound_end(0),
    data_upper_bound(0),
    capacity(max_data_capacity)
{}

size_t
header_table_t::data_size() const {
    if(data_upper_bound >= data_lower_bound) {
        return data_upper_bound - data_lower_bound;
    } else {
        return data_lower_bound_end - data_lower_bound + data_upper_bound;
    }
}

size_t
header_table_t::size() const {
    if(header_upper_bound >= header_lower_bound) {
        return header_static_table_t::size + header_upper_bound - header_lower_bound;
    } else {
        return header_static_table_t::size + headers.size() - header_lower_bound + header_upper_bound;
    }
}

size_t
header_table_t::data_capacity() const {
    return capacity;
}

bool
header_table_t::empty() const {
    return data_lower_bound == data_upper_bound;
}

void
header_table_t::push(const header_t& result) {
    size_t header_size = result.http2_size();

    // Pop headers from table until there is enough room for new one or table is empty
    while(data_size() + header_size > capacity && !empty()) {
        pop();
    }

    // Header does not fit in the table. According to RFC we just clean the table and do not put the header inside.
    if(empty() && data_size() + header_size > capacity) {
        return;
    }

    // Find the appropriate position in header table.
    char* dest = header_data.data();
    if(header_data.size() - data_upper_bound < header_size) {
        // If header does not fit in the remaining space in the end of buffer - we write at the beginning.
        // We also store point of end of the data in the end of buffer.
        // Now data starts in data_lower_bound, some header ends in data_lower_bound_end and next part will start from the beginning.
        // It is guaranteed that there is enough free space in the beginning as we've done pop before.
        data_lower_bound_end = data_upper_bound;
        // If there is no actual data, move bound to the beginning
        if(data_lower_bound == data_lower_bound_end) {
            data_lower_bound = 0;
            data_lower_bound_end = 0;
        }
    } else {
        // Ok, header fits right after previous one.
        dest += data_upper_bound;
    }

    // Encode size of the name of the header
    dest += http2_integer_encode(reinterpret_cast<unsigned char*>(dest), result.name.size, 1, 0);
    // Encode value of the name of the header (plain copy)
    std::memcpy(dest, result.name.blob, result.name.size);

    // Adjust buffer pointer
    dest += result.name.size;

    // Encode size of the value of the header
    dest += http2_integer_encode(reinterpret_cast<unsigned char*>(dest), result.value.size, 1, 0);

    // Encode value of the value of the header (plain copy)
    std::memcpy(dest, result.value.blob, result.value.size);

    // Adjust buffer pointer
    dest += result.value.size + http2_header_overhead;

    // Save header itself in header circular buffer (array) to make header navigation easier
    // It is guaranteed not to overwrite old data which is still in use, as size of dynamic table is limited.
    headers[header_upper_bound] = result;
    header_upper_bound++;
    if(header_upper_bound >= headers.size()) {
        header_upper_bound = 0;
    }

    // Store index of upper_bound of data.
    data_upper_bound = dest - header_data.data();
}

void
header_table_t::pop() {
    size_t header_size = headers[header_lower_bound].http2_size();
    data_lower_bound+=header_size;
    if(data_lower_bound == data_lower_bound_end) {
        data_lower_bound = 0;
    }

    header_lower_bound++;
    if(header_lower_bound >= headers.size()) {
        header_lower_bound = 0;
    }
}

size_t
header_table_t::find(const std::function<bool(const header_t&)> comp) {
    auto it = std::find_if(header_static_table_t::get_headers().begin(), header_static_table_t::get_headers().end(), comp);
    if(it != header_static_table_t::get_headers().end()) {
        return it - header_static_table_t::get_headers().begin();
    }
    if(header_lower_bound <= header_upper_bound) {
        auto dyn_it = std::find_if(headers.data() + header_lower_bound, headers.data() + header_upper_bound, comp);
        if(dyn_it != headers.data() + header_upper_bound) {
            return header_static_table_t::size + (dyn_it - (headers.data() + header_lower_bound));
        }
    } else {
        auto dyn_it = std::find_if(headers.data(), headers.data() + header_upper_bound, comp);
        if(dyn_it != headers.data() + header_upper_bound) {
            return header_static_table_t::size + (dyn_it - headers.data());
        }
        dyn_it = std::find_if(headers.data() + header_lower_bound, headers.end(), comp);
        if(dyn_it != headers.end()) {
            return header_static_table_t::size + (dyn_it - (headers.data() + header_lower_bound));
        }
    }
    return 0;
}

const header_t&
header_table_t::operator[](size_t idx) {
    if(idx == 0 || idx > headers.size() + header_static_table_t::size) {
        throw std::out_of_range("Invalid index for header table");
    }
    if(idx < header_static_table_t::size) {
        return header_static_table_t::get_headers()[idx];
    }
    idx -= header_static_table_t::size;
    idx += header_lower_bound;
    if(idx > headers.size()) {
        idx -= headers.size();
    }
    assert(header_upper_bound > header_lower_bound ?
               (idx >= header_lower_bound && idx < header_upper_bound) :
               (idx >= header_lower_bound || idx < header_upper_bound)
    );
    return headers[idx];
}

}} // namespace cocaine::hpack

extern "C" {

struct ch_table {
    cocaine::hpack::header_table_t table;
};

ch_table*
ch_table_init() {
    ch_table* data = new ch_table();
    return data;
}

void
ch_table_destroy(ch_table* table) {
    delete table;
}

ch_header
ch_table_get_header(ch_table* table, size_t idx) {
    try {
        auto h = table->table[idx];
        return ch_header{{h.get_name().blob, h.get_name().size}, {h.get_value().blob, h.get_value().size}};
    } catch(...) {
        return ch_header();
    }
}

void
ch_table_push(ch_table* table, const ch_header* header) {
    cocaine::hpack::header_t cpp_header(*header);
    table->table.push(cpp_header);
}

size_t
ch_table_find_by_full_match(ch_table* table, const ch_header* header) {
    cocaine::hpack::header_t cpp_header(*header);
    return table->table.find_by_full_match(cpp_header);
}

size_t
ch_table_find_by_name(ch_table* table, const ch_header* header) {
    cocaine::hpack::header_t cpp_header(*header);
    return table->table.find_by_name(cpp_header);
}

size_t
ch_table_data_size(ch_table* table) {
    return table->table.data_size();
}

size_t
ch_table_size(ch_table* table) {
    return table->table.size();
}

size_t
ch_table_data_capacity(ch_table* table) {
    return table->table.data_capacity();
}

int
ch_table_empty(ch_table* table) {
    return table->table.empty();
}

}
