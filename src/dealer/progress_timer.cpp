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

#include <cstddef>

#include "cocaine/dealer/details/progress_timer.hpp"

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
