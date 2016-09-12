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

struct another_test_header_t {
    static
    header::data_t
    name() {
        return header::create_data("another_test_name");
    }

    static
    header::data_t
    value() {
        return header::create_data("another_test_data");
    }
};

struct third_test_header_t {
    static
    header::data_t
    name() {
        return header::create_data("third_test_name");
    }

    static
    header::data_t
    value() {
        return header::create_data("third_test_data");
    }
};

}  // namespace

TEST(header_static_table_t, general) {
    const auto& headers = header_static_table_t::get_headers();
    ASSERT_EQ(headers.size(), boost::mpl::size<header_static_table_t::headers_storage>::value);

    ASSERT_EQ(header_static_table_t::idx<headers::trace_id<>>(), 80);
    ASSERT_EQ(header_static_table_t::idx<headers::span_id<>>(), 81);
    ASSERT_EQ(header_static_table_t::idx<headers::parent_id<>>(), 82);

    ASSERT_EQ(headers.at(80), headers::make_header<headers::trace_id<>>());
    ASSERT_EQ(headers.at(81), headers::make_header<headers::span_id<>>());
    ASSERT_EQ(headers.at(82), headers::make_header<headers::parent_id<>>());
}

TEST(header_table_t, operator_sq_br) {
    header_table_t table;
    // 0  is not allowed
    ASSERT_THROW(table[0], std::out_of_range);
    auto h = headers::make_header<headers::span_id<>>();
    table.push(h);
    ASSERT_EQ(headers::make_header<headers::span_id<>>(), table[81]);
}

TEST(header_table_t, push) {
    header_table_t table;
    auto h = headers::make_header<headers::span_id<>>();
    table.push(h);
    h = headers::make_header<headers::trace_id<>>();
    ASSERT_STREQ(h.get_name().blob, "trace_id");
    size_t count = (table.data_capacity() - table.data_size()) / h.http2_size();
    for(size_t i = 0; i < count; i++) {
        table.push(h);
        ASSERT_EQ(table[header_static_table_t::size], headers::make_header<headers::span_id<>>());
    }
    ASSERT_EQ(table[header_static_table_t::size], headers::make_header<headers::span_id<>>());
    table.push(h);
    ASSERT_EQ(table[header_static_table_t::size], headers::make_header<headers::trace_id<>>());
    h = headers::make_header<headers::span_id<max_stored_span_id_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.size(), header_static_table_t::get_size()+ 1);
    h = headers::make_header<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_TRUE(table.empty());
}

TEST(header_table_t, find_by_full_match) {
    header_table_t table;
    auto h1 = headers::make_header<test_header_t>();
    auto h2 = headers::make_header<test_header_t>();
    ASSERT_EQ(table.find_by_full_match(h1), 0);
    ASSERT_EQ(table.find_by_full_match(h2), 0);

    std::string data1("so much test wow");
    std::string data2("so much test wow");
    h1 = header_t::create<test_header_t>(header::data_t{data1.c_str(), data1.size()});
    h2 = header_t::create<test_header_t>(header::data_t{data2.c_str(), data2.size()});
    table.push(h1);
    data1 = "Y U no copy data?";
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size());
    data2 = "Y U no copy data?";

    h1 = header_t::create<another_test_header_t>(header::data_t{data1.c_str(), data1.size()});
    h2 = header_t::create<another_test_header_t>(header::data_t{data2.c_str(), data2.size()});
    table.push(h1);
    data1 = "Problems?";
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size() + 1);
    data2 = "Problems?";

    h1 = header_t::create<third_test_header_t>(header::data_t{data1.c_str(), data1.size()});
    h2 = header_t::create<third_test_header_t>(header::data_t{data2.c_str(), data2.size()});
    table.push(h1);
    data1 = "YAY";
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size() + 2);
    data2 = "YAY";

    h1 = header_t::create<test_header_t>(header::data_t{data1.c_str(), data1.size()});
    h2 = header_t::create<test_header_t>(header::data_t{data2.c_str(), data2.size()});
    table.push(h1);
    ASSERT_EQ(table.find_by_full_match(h2), header_static_table_t::get_size() + 3);


    auto h = headers::make_header<headers::span_id<>>();
    ASSERT_EQ(table.find_by_full_match(h), header_static_table_t::idx<headers::span_id<>>());
}

TEST(header_table_t, find_by_name) {
    header_table_t table;
    auto h = headers::make_header<test_header_t>();
    ASSERT_EQ(table.find_by_name(h), 0);

    std::string data("so much test wow");
    h = header_t::create<test_header_t>(header::data_t{data.c_str(), data.size()});
    table.push(h);
    data = "Y U no copy data?";
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size());

    h = header_t::create<another_test_header_t>(header::data_t{data.c_str(), data.size()});
    table.push(h);
    data = "Problems?";
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size() + 1);

    h = header_t::create<third_test_header_t>(header::data_t{data.c_str(), data.size()});
    table.push(h);
    data = "YAY";
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size() + 2);

    h = header_t::create<test_header_t>(header::data_t{data.c_str(), data.size()});
    table.push(h);
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::get_size());

    h = headers::make_header<headers::span_id<test_value_t>>();
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::idx<headers::span_id<>>());
    table.push(h);
    // Nothing changes. by name search should return first header from static table.
    ASSERT_EQ(table.find_by_name(h), header_static_table_t::idx<headers::span_id<>>());
}

TEST(header_table_t, data_size) {
    header_table_t table;
    ASSERT_EQ(table.data_size(), 0);

    auto h = headers::make_header<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.data_size(), 0);

    h = headers::make_header<headers::span_id<max_stored_span_id_test_value_t>>();
    table.push(h);
    ASSERT_EQ(table.data_size(), table.data_capacity());
}

TEST(header_table_t, size) {
    header_table_t table;
    ASSERT_EQ(table.size(), header_static_table_t::get_size());
    size_t counter = header_static_table_t::get_size();
    auto h = headers::make_header<headers::span_id<>>();
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
    auto h = headers::make_header<headers::span_id<>>();
    table.push(h);
    ASSERT_FALSE(table.empty());
    h = headers::make_header<headers::span_id<big_test_value_t>>();
    table.push(h);
    ASSERT_TRUE(table.empty());
}

TEST(header_table_t, complex_store_load) {
    header_table_t table;
    for(size_t i = 0; i < 10000; i++) {
        auto h = headers::make_header<headers::trace_id<>>();
        table.push(headers::make_header<headers::trace_id<>>());
        h = headers::make_header<headers::span_id<>>();
        table.push(h);
        h = headers::make_header<headers::parent_id<>>();
        table.push(h);
        h = headers::make_header<test_header_t>();
        table.push(h);
        auto idx = table.find_by_name(h);
        ASSERT_EQ(std::string(table[idx].get_name().blob, table[idx].get_name().size), "test_name");
    }

}

TEST(header_table_t, circular_header_shift) {
    header_table_t table;
    auto h = headers::make_header<test_header_t>();
    table.push(h);
    auto pos = table.find_by_name(h);
    ASSERT_EQ(pos, header_static_table_t::get_size());
    for(size_t i = 0; i < header_table_t::max_header_capacity-1; i++) {
        h = headers::make_header<headers::trace_id<>>();
        table.push(h);
    }
    h = headers::make_header<test_header_t>();
    table.push(h);
    size_t s = table.size();
    pos = table.find_by_name(h);

    ASSERT_EQ(pos, s - 1);
}

TEST(http2_integer_size,) {
    unsigned char buffer[10];
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(1, std::numeric_limits<uint64_t>::max()/2-1);
    for(char bit_offset = 1; bit_offset < 8; bit_offset++) {
        for(size_t i = 1; i< std::numeric_limits<size_t>::max() / 2; i += (dis(gen) % i +1)) {
            ASSERT_EQ(http2_integer_size(i, bit_offset), http2_integer_encode(buffer, i, bit_offset, 0));
        }
    }
}
