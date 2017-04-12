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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/lexical_cast.hpp>

namespace {

TEST(boost, lexical_cast) {
    ASSERT_TRUE(boost::lexical_cast<bool>(std::string("1")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("true")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("TRUE")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("True")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("42")));
    ASSERT_FALSE(boost::lexical_cast<bool>(std::string("0")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("FALSE")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("False")));
    EXPECT_ANY_THROW(boost::lexical_cast<bool>(std::string("false")));
}

} // namespace
