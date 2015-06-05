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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace cocaine;
using namespace cocaine::io;

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
    const char* c_str = "c_string\0This should cut as c_str is converted without size";
    auto data2 = header::create_data(c_str);
    ASSERT_EQ(data2.size, sizeof("c_string")-1);
    ASSERT_STREQ(data2.blob, c_str);
    auto data3 = header::create_data("binary\0literal");
    ASSERT_EQ(data3.size, sizeof("binary\0literal")-1);
}
