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

TEST(header_data_t, conversion) {
    size_t val = 42;
    auto data = header::pack(val);
    ASSERT_TRUE(data.size() == sizeof(val));
    ASSERT_EQ(header::unpack<size_t>(data), val);
    ASSERT_ANY_THROW(header::unpack<char>(data));
    int test2 = 42;
    ASSERT_EQ(header::unpack<int>(header::pack(test2)), test2);

    char test3 = 42;
    ASSERT_EQ(header::unpack<char>(header::pack(test3)), test3);
}

TEST(header_t, general) {
    header_t empty;
    ASSERT_EQ(empty.name(), "");
    ASSERT_EQ(empty.value(), "");
    header_t span1 = header_t::create<headers::span_id<>>();
    ASSERT_EQ(span1.name(), "span_id");
    header_t span2 = header_t::create<headers::span_id<>>();
    header_t span3(span2);
    header_t span4;
    span4 = span3;
    ASSERT_EQ(span1, span2);
    ASSERT_EQ(span2, span3);
    ASSERT_EQ(span3, span4);
}

