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

#include "cocaine/rpc/asio/header.hpp"
#include "cocaine/traits/header.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>

using namespace cocaine;
using namespace cocaine::io;

struct test_value_t {
    static
    constexpr
    header::data_t
    value() {
        return header::create_data("some different data");
    }
};


struct max_stored_span_id_test_value_t {
    static
    header::data_t
    value() {
        // 4053 is magic number when span_id header takes 4096 bytes
        static char big_data[4053] ;
        return header::create_data(big_data);
    }
};

struct big_test_value_t {
    static
    header::data_t
    value() {
        static char big_data[40960];
        return header::create_data(big_data);
    }
};

struct test_header_t {
    static
    header::data_t
    name() {
        return header::create_data("test_name");
    }

    static
    header::data_t
    value() {
        return header::create_data("test_data");
    }
};

TEST(header_data_t, comparator) {
    header::data_t data1 { "qwerty\0qw", sizeof("qwerty\0qw") };
    header::data_t data2 { "qwerty\0qw", sizeof("qwerty\0qw") };
    ASSERT_EQ(data1, data2);
}

TEST(header_data_t, conversion) {
    size_t val = 42;
    auto data = header::create_data(val);
    ASSERT_TRUE(data.size == sizeof(val));
    ASSERT_EQ(data.convert<size_t>(), val);
    ASSERT_ANY_THROW(data.convert<char>());

    int test2 = 42;
    ASSERT_EQ(header::create_data(test2).convert<int>(), test2);

    char test3 = 42;
    ASSERT_EQ(header::create_data(test3).convert<char>(), test3);

    const char* c_str = "c_string\0This should cut as c_str is converted without size";
    auto data2 = header::create_data(c_str, strlen(c_str));
    ASSERT_EQ(data2.size, sizeof("c_string")-1);
    ASSERT_STREQ(data2.blob, c_str);
    auto data3 = header::create_data("binary\0literal");
    ASSERT_EQ(data3.size, sizeof("binary\0literal")-1);
}

TEST(header_t, general) {
    header_t empty;
    ASSERT_EQ(empty.get_name().size, 0);
    ASSERT_EQ(empty.get_name().blob, nullptr);
    ASSERT_EQ(empty.get_value().size, 0);
    ASSERT_EQ(empty.get_value().blob, nullptr);
    header_t span1 = headers::make_header<headers::span_id<>>();
    ASSERT_STREQ(span1.get_name().blob, "span_id");
    ASSERT_EQ(span1.get_name().size, 7);
    header_t span2 = headers::make_header<headers::span_id<>>();
    header_t span3(span2);
    header_t span4;
    span4 = span3;
    ASSERT_EQ(span1, span2);
    ASSERT_EQ(span2, span3);
    ASSERT_EQ(span3, span4);
}

TEST(header_static_table_t, general) {
    const auto& headers = header_static_table_t::get_headers();
    ASSERT_EQ(headers.size(), boost::mpl::size<header_static_table_t::headers_storage>::value);

    ASSERT_EQ(header_static_table_t::idx<headers::trace_id<>>(), 1);
    ASSERT_EQ(header_static_table_t::idx<headers::span_id<>>(), 2);
    ASSERT_EQ(header_static_table_t::idx<headers::parent_id<>>(), 3);

    ASSERT_EQ(headers.at(1), io::headers::make_header<headers::trace_id<>>());
    ASSERT_EQ(headers.at(2), io::headers::make_header<headers::span_id<>>());
    ASSERT_EQ(headers.at(3), io::headers::make_header<headers::parent_id<>>());
}

TEST(header_table_t, operator_sq_br) {
    header_table_t table;
    // 0  is not allowed
    ASSERT_DEATH(table[0], "");
    auto h = io::headers::make_header<headers::span_id<>>();
    table.push(h);
    ASSERT_EQ(io::headers::make_header<headers::span_id<>>(), table[2]);
}

TEST(header_table_t, push) {
    header_table_t table;
    auto h = io::headers::make_header<headers::span_id<>>();
    table.push(h);
    h = io::headers::make_header<headers::trace_id<>>();
    ASSERT_STREQ(h.get_name().blob, "trace_id");
    size_t count = (table.data_capacity() - table.data_size()) / h.http2_size();
    for(size_t i = 0; i < count; i++) {
        table.push(h);
        ASSERT_EQ(table[header_static_table_t::size], io::headers::make_header<headers::span_id<>>());
    }
    ASSERT_EQ(table[header_static_table_t::size], io::headers::make_header<headers::span_id<>>());
    table.push(h);
    ASSERT_EQ(table[header_static_table_t::size], io::headers::make_header<headers::trace_id<>>());
    h = io::headers::make_header<headers::span_id<max_stored_span_id_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.size(), 1);
    h = io::headers::make_header<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_TRUE(table.empty());
}

TEST(header_table_t, find_by_full_match) {
    header_table_t table;
    auto h = io::headers::make_header<headers::span_id<>>();
    ASSERT_EQ(header_static_table_t::idx<headers::span_id<>>(), table.find_by_full_match(h));
    h = io::headers::make_header<headers::span_id<test_value_t>>();
    ASSERT_EQ(table.find_by_full_match(h), 0);
    table.push(h);
    ASSERT_EQ(table.find_by_full_match(h), header_static_table_t::get_size());
}

TEST(header_table_t, find_by_name) {
    header_table_t table;
    auto h = io::headers::make_header<test_header_t>();
    ASSERT_EQ(table.find_by_name(h), 0);

    table.push(h);
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size());
    h = io::headers::make_header<headers::span_id<test_value_t>>();
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::idx<headers::span_id<>>());
    table.push(h);
    // Nothing changes. by name search should return first header from static table.
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::idx<headers::span_id<>>());
}

TEST(header_table_t, data_size) {
    header_table_t table;
    ASSERT_EQ(table.data_size(), 0);

    auto h = io::headers::make_header<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.data_size(), 0);

    h = io::headers::make_header<headers::span_id<max_stored_span_id_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.data_size(), table.data_capacity());
}

TEST(header_table_t, size) {
    header_table_t table;
    ASSERT_EQ(table.size(), 0);
    size_t counter = 0;
    auto h = io::headers::make_header<headers::span_id<>>();
    while(true) {
        if(table.data_capacity() - table.data_size() < h.http2_size()) {
            table.push(h);
            ASSERT_EQ(table.size(), counter);
            break;
        }
        table.push(h);
        counter++;
        ASSERT_EQ(table.size(), counter);
    }
}

TEST(header_table_t, empty) {
    header_table_t table;
    ASSERT_TRUE(table.empty());
    auto h = io::headers::make_header<headers::span_id<>>();
    table.push(h);
    ASSERT_FALSE(table.empty());
    h = io::headers::make_header<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_TRUE(table.empty());
}

TEST(http2_integer_size,) {
    char buffer[10];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
    for(char bit_offset = 1; bit_offset < 8; bit_offset++) {
        for(size_t i = 1; i< std::numeric_limits<size_t>::max() / 2; i += (dis(gen) % i +1)) {
            ASSERT_EQ(http2_integer_size(i, bit_offset), http2_integer_encode(buffer, i, bit_offset, 0));
        }
    }
}
