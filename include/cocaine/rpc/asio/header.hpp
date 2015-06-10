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

#ifndef COCAINE_RPC_ASIO_HEADER_HPP
#define COCAINE_RPC_ASIO_HEADER_HPP

#include "cocaine/exceptions.hpp"
#include "cocaine/traits.hpp"

#include <boost/mpl/contains.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/vector.hpp>

#include <array>
#include <cstring>

namespace cocaine { namespace io  {
namespace header {

struct data_t {
    const char* blob;
    size_t size;
    bool operator==(const data_t& other) const {
        return size == other.size && (std::memcmp(blob, other.blob, size) == 0);
    }

    template<class To>
    To
    convert() {
        static_assert(std::is_pod<typename std::remove_reference<To>::type>::value &&
                      !std::is_pointer<typename std::remove_reference<To>::type>::value &&
                      !std::is_array<typename std::remove_reference<To>::type>::value,
                      "only POD non pointer, non array data type is allowed to convert header data"
                      );
        if(size != sizeof(typename std::remove_reference<To>::type)) {
            throw cocaine::error_t("invalid header data size");
        }
        return *(reinterpret_cast<const To*>(blob));
    }
};

template<size_t N>
inline
constexpr
data_t
create_data(char const (&source)[N]) {
    return data_t{source, N-1};
}

template<class From>
inline
constexpr
header::data_t
create_data(From&& source) {
    static_assert(std::is_lvalue_reference<From>::value &&
                  std::is_pod<typename std::remove_reference<From>::type>::value &&
                  !std::is_same<const char*, typename std::remove_reference<From>::type>::value,
                  "only lreference to POD is allowed to create header data"
                  );
    return data_t{
        reinterpret_cast<const char*>(&source),
        sizeof(typename std::remove_reference<From>::type)
    };
}

inline
constexpr
data_t
create_data(const char* source, size_t size) {
    return data_t{source, size};
}
} // namespace header

struct headers;
// Header class.
// Represents non-owning header referring to some external memory.
// Can be constructed only via header table
class header_t {
public:
    header_t(const header_t&) = default;
    header_t& operator=(const header_t&) = default;
    header_t(): name(), value() {}

    // Create non-owning header on user-provided data
    template<class Header>
    static
    header_t
    create(header::data_t _value) {
        return header_t(Header::name, _value);
    }

    inline
    header::data_t
    get_name() const;

    inline
    header::data_t
    get_value() const;

    bool
    operator==(const header_t& other) const {
        return name == other.name && value == other.value;
    }

    bool
    name_equal(const header_t& other) const {
        return name == other.name;
    }

    // Returns size of the header entry in http2 header table
    inline
    size_t
    http2_size() const;

    friend struct header_traits;
    friend struct io::headers;
    friend struct header_static_table_t;

private:
    friend class header_table_t;

    inline
    header_t(const header::data_t& _name, const header::data_t& _value) noexcept;

    inline
    header_t(const char* _name, size_t name_sz, const char* _value, size_t value_sz) noexcept;

    header::data_t name;
    header::data_t value;
};

struct headers {
    template<class Header>
    static
    header_t
    make_header() {
        return header_t(Header::name(), Header::value());
    }

    struct default_values_t {
        struct zero_uint_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("\0\0\0\0\0\0\0\0");
            }
        };

        struct empty_string_value_t {
            static
            constexpr
            header::data_t
            value() {
                return header::create_data("");
            }
        };
    };

    template<class DefaultValue = default_values_t::empty_string_value_t>
    struct empty {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("");
        }

        static
        constexpr
        header::data_t
        value() {
            return DefaultValue::value();
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct span_id {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("span_id");
        }

        static
        constexpr
        header::data_t
        value() {
            return DefaultValue::value();
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct trace_id {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("trace_id");
        }

        static
        constexpr
        header::data_t
        value() {
            return DefaultValue::value();
        }
    };

    template<class DefaultValue = default_values_t::zero_uint_value_t>
    struct parent_id {
        static
        constexpr
        header::data_t
        name() {
            return header::create_data("parent_id");
        }

        static
        constexpr
        header::data_t
        value() {
            return DefaultValue::value();
        }
    };
};

struct header_static_table_t {
    typedef boost::mpl::vector<
        headers::empty<>,
        headers::span_id<>,
        headers::trace_id<>,
        headers::parent_id<>
    > headers;

    static constexpr size_t size = boost::mpl::size<headers>::type::value;
    typedef std::array<header_t, size> storage_t;

    static
    constexpr
    size_t
    get_size() {
        return size;
    }


    static
    const storage_t&
    get_headers() {
        static storage_t storage = init_data();
        return storage;
    }

    template<class Header>
    constexpr
    static
    size_t
    idx() {
        static_assert(boost::mpl::contains<headers, Header>::type::value, "Could not find header in statis table");
        return boost::mpl::find<headers, Header>::type::pos::value;
    }

private:
    struct init_header_t {
        init_header_t(storage_t& _data) :
            data(_data)
        {}
        template<class Header>
        void
        operator()(Header) {
            data[boost::mpl::find<headers, Header>::type::pos::value].name = Header::name();
            data[boost::mpl::find<headers, Header>::type::pos::value].value = Header::value();
        }
        storage_t& data;
    };

    static
    storage_t
    init_data() {
        storage_t data;
        init_header_t init(data);
        boost::mpl::for_each<headers>(init);
        return data;
    }
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
    void
    push(header_t& header);

    inline
    size_t
    find_by_full_match(const header_t& header) {
        return find(std::bind(&header_t::operator==, &header, std::placeholders::_1));
    }

    inline
    size_t
    find_by_name(const header_t& header) {
        return find(std::bind(&header_t::name_equal, &header, std::placeholders::_1));
    }

    inline
    size_t
    data_size() const;

    inline
    size_t
    size() const;

    inline
    size_t
    data_capacity() const;

    inline
    bool
    empty() const;

    static constexpr size_t max_data_capacity = 4096;
    static constexpr size_t http2_header_overhead = 32;
    //32 bytes overhead per record and 2 bytes for nil-nil header
    static constexpr size_t max_header_capacity = max_data_capacity / (http2_header_overhead + 2);

private:
    inline
    void
    pop();

    inline
    size_t
    find(const std::function<bool(const header_t&)> comp);

    // Header storage. Implemented as circular buffer
    std::array<header_t, max_header_capacity> headers;
    size_t header_lower_bound;
    size_t header_upper_bound;

    // Header data storage. Stores all data which headers can reference.
    // Implemented as a sort of circular buffer.
    // We multiply by 2 as data can be padded and we don't want to move it in memory.
    // 2 multiplier guarantee that we can add new value to the end or beginning without data overlap.
    std::array<char, max_data_capacity*2> header_data;
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

// --------------------------------------------------
// Implementation part. Header-only to be used in CNF
// --------------------------------------------------

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
        return header_upper_bound - header_lower_bound;
    } else {
        return headers.size() - header_lower_bound + header_upper_bound;
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
header_table_t::push(header_t& result) {
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
    dest += http2_integer_encode(dest, result.name.size, 1, 0);
    // Encode value of the name of the header (plain copy)
    std::memcpy(dest, result.name.blob, result.name.size);

    // Make header name point to data in the tabel. Size already was there. It did not change.
    result.name.blob = dest;

    // Adjust buffer pointer
    dest += result.name.size;

    // Encode size of the value of the header
    dest += http2_integer_encode(dest, result.value.size, 1, 0);

    // Encode value of the value of the header (plain copy)
    std::memcpy(dest, result.value.blob, result.value.size);

    // Make header value point to data in the table
    result.value.blob = dest;

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
    assert(idx != 0);
    assert(idx < headers.size());
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

size_t
http2_integer_size(size_t sz, size_t bit_offset) {
    // See packing here https://httpwg.github.io/specs/rfc7541.html#integer.representation
    // if integer fits to 8 - bit_offset bits
    if(sz < (1 << (8 - bit_offset))) {
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
http2_integer_encode(char* dest, size_t source, size_t bit_offset, char prefix) { 
    dest[0] = prefix;
    unsigned char mask = 255;
    mask = ~(mask >> bit_offset);
    dest[0] &= mask;
    size_t first_byte_cap = 1 << (8-bit_offset);
    if(source < first_byte_cap) {
        dest[0] += source;
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

}}

#endif // COCAINE_RPC_ASIO_HEADER_HPP


