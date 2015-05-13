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

#ifndef HEADER_HPP
#define HEADER_HPP

#include "cocaine/traits.hpp"
#include "cocaine/exceptions.hpp"

#include <cstring>
#include <array>
#include <cassert>
#include <functional>

namespace cocaine { namespace io  {

enum class header_id {
    TRACE_ID,
    SPAN_ID,
    PARENT_ID,
    //Not used. Just indicates size of headers.
    LAST
};

// Represents header name. Can be constructed only from headers enum.
class header_key_t {
public:
    constexpr
    header_key_t(header_id _id) :
        id(_id)
    {}

    inline
    const std::string&
    name() const {
        return names()[static_cast<size_t>(id)];
    }

private:
    header_id id;
    static constexpr size_t header_cnt = static_cast<size_t>(header_id::LAST);

    inline
    static
    const std::array<std::string, header_cnt>&
    names() {
        static std::array<std::string, header_cnt> header_names = {
            "trace_id",
            "span_id",
            "parent_id"
        };
        return header_names;
    }
};

// Header class.
// Represents non-owning header referring to some external memory.
// Can be constructed only via header table
class header_t {
public:
    struct str {
        const char* data;
        size_t size;
        bool operator==(const str& other) const {
            return size == other.size && (memcmp(data, other.data, size) == 0);
        }
    };

    header_t(const header_t& other) = default;
    header_t& operator=(const header_t& other) = default;

    inline
    header_t(header_key_t key, str _value) noexcept :
        name({key.name().c_str(), key.name().size()}),
        value(_value)
    {}

    inline
    str
    get_name() const;

    inline
    str
    get_string_value() const;

    // Just a convience conversion of chunk of data to uint64_t.
    // Header has no type information, so
    // it's up to user to determine what he expects to see - string or integer.
    inline
    uint64_t
    get_numeric_value() const;

    inline
    header_t() noexcept = default;

    bool operator==(const header_t& other) const {
        return name == other.name && value == other.value;
    }

    bool name_equal(const header_t& other) const {
        return name == other.name;
    }

private:
    inline
    header_t(const char* _name, size_t name_sz, const char* _value, size_t value_sz) noexcept;

    inline
    header_t(str _name, str _value) noexcept;

    friend class header_table_t;

    // Returns size of the header entry in http2 header table
    inline
    size_t
    http2_size() const;

    str name;
    str value;
};

// Header static and dynamic table as described in http2
// See https://tools.ietf.org/html/draft-ietf-httpbis-header-compression-12#section-2.3
class header_table_t {
public:
    inline
    header_table_t();

    inline
    const header_t&
    operator[](size_t idx);

    inline
    bool
    parse_headers(std::vector<header_t>& parse_to, const msgpack::object& parse_from);

    inline
    header_t
    push(const msgpack::object& data);

    inline
    void
    push(header_t& header);

    template<class Packer>
    void pack(Packer& packer, header_t header) {
        size_t pos = find_full(header);
        if(pos) {
            packer.pack_fix_uint64(pos);
            return;
        }
        packer.pack_array(3);
        pos = find_name(header);
        packer.pack_true();
        push(header);
        if(pos) {
            packer.pack_fix_uint64(pos);
        }
        else {
            packer.pack_raw(header.name.size);
            packer.pack_raw_body(header.name.data, header.name.size);
        }
        packer.pack_raw(header.value.size);
        packer.pack_raw_body(header.value.data, header.value.size);
    }

    inline
    size_t
    size() const;

    inline
    bool
    empty() const;

    static constexpr const size_t MAX_DATA_CAPACITY = 4096;

    //32 bytes overhead per record and 2 bytes for nil-nil header
    static constexpr const size_t MAX_HEADER_CAPACITY = MAX_DATA_CAPACITY/34;
private:
    inline
    static const std::vector<header_t>&
    static_data();

    inline
    void
    pop();

    size_t find_full(const header_t& header) {
        return find(std::bind(&header_t::operator==, &header, std::placeholders::_1));
    }

    size_t find_name(const header_t& header) {
        return find(std::bind(&header_t::name_equal, &header, std::placeholders::_1));
    }

    size_t find(const std::function<bool(const header_t&)> comp) {
        auto it = std::find_if(static_data().begin(), static_data().end(), comp);
        if(it != static_data().end()) {
            return it - static_data().begin();
        }
        if(header_lower_bound <= header_upper_bound) {
            auto dyn_it = std::find_if(headers.data() + header_lower_bound, headers.data() + header_upper_bound, comp);
            if(dyn_it != headers.data() + header_upper_bound) {
                return static_data().size() - 1 + (dyn_it - (headers.data() + header_lower_bound));
            }
        }
        else {
            auto dyn_it = std::find_if(headers.data(), headers.data() + header_upper_bound, comp);
            if(dyn_it != headers.data() + header_upper_bound) {
                return static_data().size() - 1 + (dyn_it - headers.data());
            }
            dyn_it = std::find_if(headers.data() + header_lower_bound, headers.end(), comp);
            if(dyn_it != headers.end()) {
                return static_data().size() - 1 + (dyn_it - (headers.data() + header_lower_bound));
            }
        }
        return 0;
    }



    // Header storage. Implemented as circular buffer
    std::array<header_t, MAX_HEADER_CAPACITY> headers;
    size_t header_lower_bound;
    size_t header_upper_bound;

    // Header data storage. Stores all data which headers can reference.
    // Implemented as a sort of circular buffer.
    // We multiply by 2 as data can be padded and we don't want to move it in memory
    std::array<char, MAX_DATA_CAPACITY*2> header_data;
    size_t data_lower_bound;
    size_t data_lower_bound_end;
    size_t data_upper_bound;
    size_t capacity;
};

// Determines size of integer encoded via http2 binary protocol.
inline
size_t
http2_integer_size(size_t sz, size_t bit_offset);


// Encodes integer to dest via http2 binary protocol.
// Note dest should be at least 10 bytes long.

inline
size_t
http2_integer_encode(char* dest, size_t source, size_t bit_offset, char prefix);

inline
bool
operator==(const header_key_t& header_key, const header_t::str& str);

inline
bool
operator==(const header_t::str& str, const header_key_t& header_key);

// --------------------------------------------------
// Implementation part. Header-only to be used in CNF
// --------------------------------------------------

header_t::header_t(str _name, str _value) noexcept :
    name(_name),
    value(_value)
{}

header_t::header_t(const char* _name, size_t name_sz, const char* _value, size_t value_sz) noexcept :
    name({_name, name_sz}),
    value({_value, value_sz})
{}

header_t::str
header_t::get_name() const {
    return name;
}

header_t::str
header_t::get_string_value() const {
    return value;
}

uint64_t
header_t::get_numeric_value() const {
    return *reinterpret_cast<const uint64_t*>(value.data);
    /*
    uint64_t res;
    memcpy(&res, value.data, 8);
    return res;
    */
}

size_t
header_t::http2_size() const {
    // 1 refer to string literals which has size with 1-bit padding.
    // See https://tools.ietf.org/html/draft-ietf-httpbis-header-compression-12#section-5.2
    return name.size + http2_integer_size(name.size, 1) + value.size + http2_integer_size(value.size, 1);
}
header_table_t::header_table_t() :
    header_lower_bound(0),
    header_upper_bound(0),
    data_lower_bound(0),
    data_upper_bound(0),
    capacity(MAX_DATA_CAPACITY)
{}

size_t
header_table_t::size() const {
    if(data_upper_bound > data_lower_bound) {
        return data_upper_bound - data_lower_bound;
    }
    else {
        return data_lower_bound_end - data_lower_bound + data_upper_bound;
    }
}

bool header_table_t::empty() const {
    return data_lower_bound == data_upper_bound;
}


header_t
header_table_t::push(const msgpack::object& data) {
    header_t result;
    //If header is fully from the table just fill it and return
    if(data.type == msgpack::type::POSITIVE_INTEGER) {
        if(data.via.u64 >= size() || data.via.u64 == 0) {
            throw cocaine::error_t("Invalid index for header table");
        }
        result = operator [](data.via.u64);
        return result;
    }

    // Encode name to header
    if(data.via.array.ptr[1].type == msgpack::type::POSITIVE_INTEGER) {
        result.name = operator [](data.via.array.ptr[1].via.u64).name;
    }
    else {
        result.name.data = data.via.array.ptr[1].via.raw.ptr;
        result.name.size = data.via.array.ptr[1].via.raw.size;
    }

    // Encode value to header
    auto value = data.via.array.ptr[2];
    result.value.data = value.via.raw.ptr;
    result.value.size = value.via.raw.size;
    //We don't need to store header in the table
    if(!data.via.array.ptr[0].via.boolean) {
        return result;
    }
    push(result);
}

void
header_table_t::push(header_t& result) {

    // Proceed to encode header to table
    size_t value_size_size = http2_integer_size(result.value.size, 1);
    size_t value_size = result.value.size;
    size_t header_size = 32 + result.name.size + http2_integer_size(result.name.size, 1) + value_size_size + value_size;

    //pop headers from table until there is enough room for new one
    while(data_upper_bound - data_lower_bound + header_size > capacity && data_lower_bound != data_upper_bound) {
        pop();
    }

    //header do not fit in the table
    if(empty() && data_upper_bound - data_lower_bound + header_size > capacity) {
        return result;
    }
    char* dest = header_data.data();
    if(header_data.size() - data_upper_bound < header_size) {
        data_lower_bound_end = data_upper_bound;
    }
    else {
        dest += data_upper_bound;
    }
    dest += http2_integer_encode(dest, result.name.size, 1, 0);
    memcpy(dest, result.name.data, result.name.size);
    result.name.data = dest;
    dest += result.name.size;
    dest += http2_integer_encode(dest, value_size, 1, 0);
    memcpy(dest, result.value.data, value_size);
    result.value.size = value_size;
    result.value.data = dest;
    dest += value_size;
    headers[header_upper_bound] = result;
    header_upper_bound++;
    if(header_upper_bound >= headers.size()) {
        header_upper_bound = 0;
    }

    data_upper_bound = dest - header_data.data();
    return result;
}

void
header_table_t::pop() {
    size_t header_size = headers[header_lower_bound].http2_size();
    header_lower_bound++;
    if(header_lower_bound >= headers.size()) {
        header_lower_bound = 0;
    }
    data_lower_bound+=header_size;
    if(data_lower_bound == data_lower_bound_end) {
        data_lower_bound = 0;
    }
}

const header_t&
header_table_t::operator[](size_t idx) {
    assert(idx != 0);
    if(idx < static_data().size()) {
        return static_data()[idx];
    }
    idx -= static_data().size();
    idx += header_lower_bound;
    if(idx > headers.size()) {
        idx -= headers.size();
    }
    assert(header_upper_bound > header_lower_bound ?
               (idx >= header_lower_bound && idx < header_upper_bound) :
               (idx >= header_lower_bound || idx < header_upper_bound)
    );
}

bool
header_table_t::parse_headers(std::vector<header_t> &parse_to, const msgpack::object &parse_from) {
    if(parse_from.via.array.size < 4) {
        return true;
    }
    parse_to.reserve(parse_from.via.array.ptr[3].via.array.size);
    for (size_t i = 0; i < parse_from.via.array.ptr[3].via.array.size; i++) {
        msgpack::object& obj = parse_from.via.array.ptr[3].via.array.ptr[i];
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
                parse_to.push_back(push(obj));
            }
            catch(const cocaine::error_t& e) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    return true;
}

#define COCAINE_STATIC_HEADER(name, val) header_t(name, sizeof(name)-1, val, sizeof(val)-1)
#define COCAINE_STATIC_EMPTY_HEADER(name) COCAINE_STATIC_HEADER(name, "")
const std::vector<header_t>&
header_table_t::static_data() {
    static std::vector<header_t> headers ({
        //Empty header is reserved as 0 is not valid index in http2
        COCAINE_STATIC_EMPTY_HEADER(""),
        COCAINE_STATIC_HEADER("trace_id", "\0\0\0\0\0\0\0\0"),
        COCAINE_STATIC_HEADER("span_id", "\0\0\0\0\0\0\0\0"),
        COCAINE_STATIC_HEADER("parent_id", "\0\0\0\0\0\0\0\0")
    });

    return headers;
}

bool
operator==(const header_key_t& header_key, const header_t::str& str) {
    //fast check first
    if(header_key.name().size() != str.size) {
        return false;
    }
    return header_key.name() == str.data;
}

bool
operator==(const header_t::str& str, const header_key_t& header_key) {
    return operator==(header_key, str);
}

size_t
http2_integer_size(size_t sz, size_t bit_offset) {
    if(sz < (1 << (8 - bit_offset))) {
        return 1;
    }
    size_t ret = 1;
    sz -= (1 << (8 - bit_offset));
    while(sz > 127) {
        sz << 7;
        ret++;
    }
    return ret;
}

//Note dest should be at least 10 bytes long
size_t
http2_integer_encode(char* dest, size_t source, size_t bit_offset, char prefix) {
    dest[0] = prefix;
    char mask = 255;
    mask = ~(mask >> bit_offset);
    dest[0] &= mask;
    char first_byte_cap = 1 << (8-bit_offset);
    if(source < first_byte_cap) {
        dest[0] += source;
        return 1;
    }
    dest[0] += first_byte_cap;
    source -= first_byte_cap;
    size_t ret = 2;
    while(source > 127) {
        dest[ret-1] = 128 + source % 128;
        source/=128;
        ret++;
    }
    dest[ret-1] = source;
    return ret;
}

}}

#endif // HEADER_HPP

