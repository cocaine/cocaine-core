/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include <cstddef>

#include "cocaine/dealer/utils/progress_timer.hpp"

namespace cocaine {
namespace dealer {

progress_timer::progress_timer() {
	begin_.init_from_current_time();
}

progress_timer::~progress_timer() {

}

void
progress_timer::reset() {
	begin_.init_from_current_time();
}

time_value
progress_timer::elapsed() {
	time_value curr_time = time_value::get_current_time();

	if (begin_ > curr_time) {
		return time_value();
	}

	time_value retval(curr_time.distance(begin_));
	return retval;
}
	
} // namespace dealer
} // namespace cocaine
