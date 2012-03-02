//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define BOOST_AUTO_TEST_MAIN

#include <boost/mpl/list.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/thread.hpp>

#include "details/time_value.hpp"

typedef boost::mpl::list<int, long, unsigned char> test_types;

BOOST_AUTO_TEST_SUITE(test_cached_read);

BOOST_AUTO_TEST_CASE_TEMPLATE(time_value_test1, T, test_types) {
	lsd::time_value tv;
	BOOST_CHECK_EQUAL(tv.as_double(), 0.0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(time_value_test2, T, test_types) {
	lsd::time_value tv, tv2;
	tv.init_from_current_time();
	tv2 = tv;

	BOOST_CHECK_EQUAL(tv == tv2, true);
	BOOST_CHECK_EQUAL(tv != tv2, false);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(time_value_test3, T, test_types) {
	lsd::time_value tv1, tv2;
	tv1.init_from_current_time();

	float t1 = tv1.as_double();
	tv2 = tv1 + 1.5;
	float t2 = tv2.as_double();

	BOOST_CHECK_EQUAL(tv2 == tv1 + 1.5, true);
	BOOST_CHECK_EQUAL(tv2 != tv1 + 1.5, false);

	double distance = tv2.distance(tv1) - 1.5;
	BOOST_CHECK_EQUAL(distance < 0.00000001, true);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(time_value_test4, T, test_types) {
	lsd::time_value tv1(136416213.5), tv2;
	tv2 = tv1 + 21.0003;

	BOOST_CHECK_EQUAL(tv1.days(), tv2.days());
	BOOST_CHECK_EQUAL(tv1.hours(), tv2.hours());
	BOOST_CHECK_EQUAL(tv1.minutes(), tv2.minutes());
	BOOST_CHECK_EQUAL(tv1.seconds() == tv2.seconds(), false);
	BOOST_CHECK_EQUAL(tv1.milliseconds(), tv2.milliseconds() - 21000);
	BOOST_CHECK_EQUAL(tv1.microseconds() == tv2.microseconds(), false);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(time_value_test5, T, test_types) {
	lsd::time_value tv1(136416213.5), tv2;
	tv2 += (tv1 + 1.5).as_double();

	BOOST_CHECK_EQUAL(tv2 == tv1 + 1.5, true);
	BOOST_CHECK_EQUAL(tv2 != tv1 + 1.5, false);
	BOOST_CHECK_EQUAL(tv2 > tv1, true);
	BOOST_CHECK_EQUAL(tv1 < tv2, true);
}

BOOST_AUTO_TEST_SUITE_END();
