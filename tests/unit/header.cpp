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
#include <cocaine/hpack/static_table.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>

using namespace cocaine::hpack;

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

TEST(header_t, zone) {
    header_t h1 = headers::make_header<headers::method<headers::default_values_t::get_value_t>>();
    header_t h2 = h1;
    std::vector<header_t> headers;
    headers.push_back(h2);
    headers.push_back(h2);

    ASSERT_TRUE(h1 == h2);
    ASSERT_EQ(h1, h2);
    ASSERT_EQ(h2, headers[0]);
    ASSERT_EQ(headers[0], headers[1]);
    header_t::zone_t zone1(h2);
    header_t::zone_t zone2(headers);
    ASSERT_EQ(h1, h2);
    ASSERT_EQ(h2, headers[0]);
    ASSERT_EQ(headers[0], headers[1]);
    ASSERT_NE(h1.get_name().blob, h2.get_name().blob);
    ASSERT_NE(h1.get_value().blob, h2.get_value().blob);
    ASSERT_NE(h2.get_name().blob, headers[0].get_name().blob);
    ASSERT_NE(h2.get_value().blob, headers[0].get_value().blob);
    ASSERT_NE(headers[0].get_name().blob, headers[1].get_name().blob);
    ASSERT_NE(headers[0].get_value().blob, headers[1].get_value().blob);
}

TEST(header_storage_t, copy) {
    header_t h1 = headers::make_header<headers::method<headers::default_values_t::get_value_t>>();
    header_t h2 = h1;
    std::vector<header_t> headers;
    headers.push_back(h2);
    headers.push_back(h2);
    header_storage_t storage(headers);
    header_storage_t storage2;
    storage2 = storage;
    ASSERT_EQ(storage2.get_headers().front(), h1);
    ASSERT_NE(storage2.get_headers().front().get_name().blob, h1.get_name().blob);
}
