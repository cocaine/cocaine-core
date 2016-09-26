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

#include <array>
#include <deque>
#include <functional>
#include <string>
#include <system_error>
#include <vector>

#include <boost/optional/optional.hpp>

struct ch_header;

namespace cocaine { namespace hpack {

struct init_header_t;
class header_t;

size_t
http2_integer_size(size_t sz, size_t bit_offset);

namespace header {

template<size_t N>
std::string
pack(char const (&source)[N]) {
    return std::string{source, N-1};
}

template<class From>
std::string
pack(From&& source) {
    static_assert(std::is_pod<typename std::remove_reference<From>::type>::value &&
                  !std::is_pointer<typename std::remove_reference<From>::type>::value,
                  "only lreference to non-pointer POD is allowed to pack header data");

    return std::string {
        reinterpret_cast<const char*>(&source),
        sizeof(typename std::remove_reference<From>::type)
    };
}

inline
std::string
pack(const char* source, size_t size) {
    return std::string(source, size);
}

template<class To>
To
unpack(const std::string& from) {
    static_assert(std::is_pod<typename std::remove_reference<To>::type>::value &&
                  !std::is_pointer<typename std::remove_reference<To>::type>::value &&
                  !std::is_array<typename std::remove_reference<To>::type>::value,
                  "only POD non pointer, non array data type is allowed to convert header data"
    );
    if(from.size() != sizeof(typename std::remove_reference<To>::type)) {
        throw std::system_error(std::make_error_code(std::errc::invalid_argument), "invalid header data size");
    }
    return *(reinterpret_cast<const To*>(from.c_str()));
}

boost::optional<const header_t&>
find_first(const std::vector<header_t>& headers, const std::string& name);

boost::optional<const header_t&>
find_first(const std::vector<header_t>& headers, const char* name, size_t sz);

template<size_t N>
boost::optional<const header_t&>
find_first(const std::vector<header_t>& headers, char const (&name)[N]) {
    return find_first(headers, name, N);
}

template<class Header>
boost::optional<const header_t&>
find_first(const std::vector<header_t>& headers) {
    return find_first(headers, Header::name());
}

template <class To, class From>
boost::optional<To>
convert_first(const std::vector<header_t>& headers, From&& from) {
    if(auto v = find_first(headers, from)) {
        return boost::make_optional(unpack<To>(v->value()));
    }
    return boost::none;
}

} // namespace header

struct headers;

// Header class.
class header_t {
public:
    header_t() = default;
    header_t(std::string name, std::string value);

    // Create predefined header on user-provided data
    template<class Header>
    static
    header_t
    create(std::string _value) {
        return header_t(Header::name(), _value);
    }

    // Create predefined header on user-provided data
    template<class Header>
    static
    header_t
    create() {
        return header_t(Header::name(), Header::value());
    }

    const std::string&
    name() const;

    const std::string&
    value() const;

    bool
    operator==(const header_t& other) const;

    bool
    name_equal(const header_t& other) const;

    // Returns size of the header entry in http2 header table
    size_t
    http2_size() const;

private:
    struct {
        std::string name;
        std::string value;
    } data;
};

typedef std::vector<header_t> header_storage_t;

// Header static and dynamic table as described in http2
// See https://tools.ietf.org/html/draft-ietf-httpbis-header-compression-12#section-2.3
class header_table_t {
public:
    header_table_t();

    const header_t&
    operator[](size_t idx);

    void
    push(header_t header);

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

    size_t
    data_size() const;

    size_t
    data_capacity() const;

    size_t
    size() const;

    bool
    empty() const;



    static constexpr size_t max_data_capacity = 4096;
    static constexpr size_t http2_header_overhead = 32;
    //32 bytes overhead per record and 2 bytes for nil-nil header
    static constexpr size_t max_header_capacity = max_data_capacity / (http2_header_overhead + 2);

private:
    void
    pop();

    size_t
    find(const std::function<bool(const header_t&)> comp);

    // Header storage. Implemented as circular buffer
    std::deque<header_t> headers;
    size_t capacity;
};

}} // namespace cocaine::hpack
