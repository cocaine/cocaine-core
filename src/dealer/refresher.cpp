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

#include <boost/bind.hpp>

#include "cocaine/dealer/utils/refresher.hpp"

namespace cocaine {
namespace dealer {

refresher::refresher(boost::function<void()> f, boost::uint32_t timeout) :
	m_func(f),
	m_timeout(timeout),
	m_stopping(false),
	m_refreshing_thread(boost::bind(&refresher::refreshing_thread, this)) {
}

refresher::~refresher() {
	m_stopping = true;
	m_cond_var.notify_one();
	m_refreshing_thread.join();
}

void
refresher::refreshing_thread() {
	if (!m_stopping && m_func) {
		m_func();
	}

	while (!m_stopping) {
		boost::mutex::scoped_lock lock(m_mutex);
		
		unsigned long long millisecs = static_cast<unsigned long long>(m_timeout);
		boost::system_time t = boost::get_system_time() + boost::posix_time::milliseconds(millisecs);
		m_cond_var.timed_wait(lock, t);

		if (!m_stopping && m_func) {
			m_func();
		}
	}
}

} // namespace dealer
} // namespace cocaine
