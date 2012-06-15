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

#include <iostream>
#include <boost/bind.hpp>

#include "cocaine/dealer/utils/refresher.hpp"

namespace cocaine {
namespace dealer {

refresher::refresher(boost::function<void()> f, boost::uint32_t timeout) :
	f_(f),
	timeout_(timeout),
	stopping_(false),
	refreshing_thread_(boost::bind(&refresher::refreshing_thread, this)) {
}

refresher::~refresher() {
	stopping_ = true;
	refreshing_thread_.join();
}

void
refresher::refreshing_thread() {
	if (!stopping_ && f_) {
		f_();
	}

	while (!stopping_) {
		boost::mutex::scoped_lock lock(m_mutex);
		
		for (int i = 0; i < 100; ++i) {
			boost::this_thread::sleep(boost::posix_time::milliseconds(timeout_ / 100));

			if (stopping_) {
				break;
			}
		}
		
		if (!stopping_ && f_) {
			f_();
		}
	}
}

} // namespace dealer
} // namespace cocaine
