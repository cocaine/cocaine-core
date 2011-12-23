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

#include <iostream>
#include <boost/bind.hpp>

#include "cocaine/dealer/details/refresher.hpp"

namespace cocaine {
namespace dealer {

refresher::refresher(boost::function<void()> f, boost::uint32_t timeout_seconds) :
	f_(f),
	timeout_(timeout_seconds),
	stopping_(false),
	refreshing_thread_(boost::bind(&refresher::refreshing_thread, this)) {
}

refresher::~refresher() {
	stopping_ = true;
	condition_.notify_one();
	refreshing_thread_.join();
}

void
refresher::refreshing_thread() {
	if (!stopping_ && f_) {
		f_();
	}

	while (!stopping_) {
		boost::mutex::scoped_lock lock(mutex_);
		boost::xtime t;
		boost::xtime_get(&t, boost::TIME_UTC);
		
		t.sec += timeout_;
		condition_.timed_wait(lock, t);
		
		if (!stopping_ && f_) {
			f_();
		}
	}
}

} // namespace dealer
} // namespace cocaine
