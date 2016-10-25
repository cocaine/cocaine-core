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

#include <cocaine/hpack/header.hpp>
#include <cocaine/hpack/msgpack_traits.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>

using namespace cocaine::hpack;

namespace {

struct test_value_t {
    static
    const std::string&
    value() {
        static std::string data("some different data");
        return data;
    }
};


struct max_stored_span_id_test_value_t {
    static
    const std::string&
    value() {
        // 4057 is magic number when span_id header takes 4096 bytes
        // 4057 == 4096 - 32 - len("span_id")
        static std::string big_data(4057, 'x');
        return big_data;
    }
};

struct big_test_value_t {
    static
    const std::string&
    value() {
        static std::string big_data(40960, 'x');
        return big_data;
    }
};

struct test_header_t {
    static
    const std::string&
    name() {
        static std::string data("test_name");
        return data;
    }

    static
    const std::string&
    value() {
        static std::string data("test_data");
        return data;
    }
};

struct another_test_header_t {
    static
    const std::string&
    name() {
        static std::string data("another_test_name");
        return data;
    }

    static
    const std::string&
    value() {
        static std::string data("another_test_data");
        return data;
    }
};

struct third_test_header_t {
    static
    const std::string&
    name() {
        static std::string data("third_test_name");
        return data;
    }

    static
    const std::string&
    value() {
        static std::string data("third_test_data");
        return data;
    }
};

}  // namespace

TEST(header_static_table_t, general) {
    const auto& headers = header_static_table_t::get_headers();
    ASSERT_EQ(headers.size(), boost::mpl::size<header_static_table_t::headers_storage>::value);

    ASSERT_EQ(header_static_table_t::idx<headers::trace_id<>>(), 80);
    ASSERT_EQ(header_static_table_t::idx<headers::span_id<>>(), 81);
    ASSERT_EQ(header_static_table_t::idx<headers::parent_id<>>(), 82);

    ASSERT_EQ(headers.at(80), header_t::create<headers::trace_id<>>());
    ASSERT_EQ(headers.at(81), header_t::create<headers::span_id<>>());
    ASSERT_EQ(headers.at(82), header_t::create<headers::parent_id<>>());
}

TEST(header_table_t, operator_sq_br) {
    header_table_t table;
    // 0  is not allowed
    ASSERT_THROW(table[0], std::out_of_range);
    auto h = header_t::create<headers::span_id<>>();
    table.push(h);
    ASSERT_EQ(header_t::create<headers::span_id<>>(), table[81]);
}

TEST(header_table_t, push) {
    header_table_t table;
    auto h = header_t::create<headers::span_id<>>();
    table.push(h);
    h = header_t::create<headers::trace_id<>>();
    ASSERT_EQ(h.name(), "trace_id");
    size_t count = (table.data_capacity() - table.data_size()) / h.http2_size();
    for(size_t i = 0; i < count; i++) {
        table.push(h);
        ASSERT_EQ(table[header_static_table_t::size + i + 1], header_t::create<headers::span_id<>>());
        ASSERT_EQ(table[header_static_table_t::size], header_t::create<headers::trace_id<>>());
    }
    ASSERT_EQ(table[header_static_table_t::size + count], header_t::create<headers::span_id<>>());
    table.push(h);
    ASSERT_EQ(table[header_static_table_t::size + count], header_t::create<headers::trace_id<>>());
    table.push( header_t::create<headers::span_id<>>());
    h = header_t::create<headers::span_id<max_stored_span_id_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.size(), header_static_table_t::get_size()+ 1);
    h = header_t::create<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_TRUE(table.empty());
}

TEST(header_table_t, find_by_full_match) {
    header_table_t table;
    auto h1 = header_t::create<test_header_t>();
    auto h2 = header_t::create<test_header_t>();
    ASSERT_EQ(table.find_by_full_match(h1), 0);
    ASSERT_EQ(table.find_by_full_match(h2), 0);

    h1 = header_t::create<test_header_t>("so much test wow");
    h2 = header_t::create<test_header_t>("so much test wow");
    table.push(h1);
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size());

    h1 = header_t::create<another_test_header_t>("such much!");
    h2 = header_t::create<another_test_header_t>("such much!");
    table.push(h1);
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size());

    h1 = header_t::create<third_test_header_t>("WOW");
    h2 = header_t::create<third_test_header_t>("WOW");
    table.push(h1);
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size());

    h1 = header_t::create<test_header_t>("YAY");
    h2 = header_t::create<test_header_t>("YAY");
    table.push(h1);
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size());

    auto h = header_t::create<headers::span_id<>>();
    ASSERT_EQ(table.find_by_full_match(h), header_static_table_t::idx<headers::span_id<>>());
}

TEST(header_table_t, find_by_name) {
    header_table_t table;
    auto h = header_t::create<test_header_t>();
    ASSERT_EQ(table.find_by_name(h), 0);

    table.push(h);
    h = header_t::create<test_header_t>("so much test wow");
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size());

    h = header_t::create<another_test_header_t>();
    table.push(h);
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size());

    h = header_t::create<third_test_header_t>();
    table.push(h);
    h = header_t::create<third_test_header_t>("such test, test much");
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size());

    h = header_t::create<headers::span_id<test_value_t>>();
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::idx<headers::span_id<>>());
    table.push(h);
    // Nothing changes. by name search should return first header from static table.
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::idx<headers::span_id<>>());
}

TEST(header_table_t, data_size) {
    header_table_t table;
    ASSERT_EQ(table.data_size(), 0);

    auto h = header_t::create<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.data_size(), 0);
    ASSERT_TRUE(table.empty());

    h = header_t::create<headers::span_id<max_stored_span_id_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.data_size(), table.data_capacity());
}

TEST(header_table_t, size) {
    header_table_t table;
    ASSERT_EQ(table.size(), header_static_table_t::get_size());
    size_t counter = header_static_table_t::get_size();
    auto h = header_t::create<headers::span_id<>>();
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
    auto h = header_t::create<headers::span_id<>>();
    table.push(h);
    ASSERT_FALSE(table.empty());
    h = header_t::create<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_TRUE(table.empty());
}

TEST(header_table_t, complex_store_load) {
    header_table_t table;
    for(size_t i = 0; i < 10000; i++) {
        auto h = header_t::create<headers::trace_id<>>();
        table.push(header_t::create<headers::trace_id<>>());
        h = header_t::create<headers::span_id<>>();
        table.push(h);
        h = header_t::create<headers::parent_id<>>();
        table.push(h);
        h = header_t::create<test_header_t>();
        table.push(h);
        auto idx = table.find_by_name(h);
        ASSERT_EQ(table[idx].name(), "test_name");
    }
}

