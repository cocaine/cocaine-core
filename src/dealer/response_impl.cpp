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

#include <stdexcept>
#include <iostream>

#include <boost/current_function.hpp>
#include <boost/bind.hpp>

#include "cocaine/dealer/dealer.hpp"
#include <cocaine/dealer/core/response_impl.hpp>
#include <cocaine/dealer/core/dealer_impl.hpp>
#include <cocaine/dealer/utils/error.hpp>

namespace cocaine {
namespace dealer {

response_impl::response_impl(const boost::shared_ptr<dealer_impl_t>& client_ptr, const std::string& uuid, const message_path& path) :
	dealer_m(client_ptr),
	uuid_m(uuid),
	path_m(path),
	response_finished_m(false),
	message_finished_m(false),
	caught_error_m(false)
{
	assert(client_ptr.get() != NULL);
}

response_impl::~response_impl() {
	boost::mutex::scoped_lock lock(mutex_m);
	message_finished_m = true;
	response_finished_m = true;
	chunks_m.clear();

	boost::shared_ptr<dealer_impl_t> dealer_ptr = dealer_m.lock();
	if (dealer_ptr) {
		dealer_ptr->unset_response_callback(uuid_m, path_m);
	}	
}

bool
response_impl::get(data_container* data, double timeout) {
	boost::mutex::scoped_lock lock(mutex_m);

	// no more chunks?
	if (message_finished_m && chunks_m.size() == 0) {
		if (!caught_error_m) {
			return false;
		}
	}

	// block until received callback
	if (!message_finished_m) {
		while (!response_finished_m && !message_finished_m) {
			if (timeout < 0.0) {
				cond_var_m.wait(lock);
			}
			else {
				unsigned long long millisecs = static_cast<unsigned long long>(timeout * 1000000);
				boost::system_time t = boost::get_system_time() + boost::posix_time::milliseconds(millisecs);
				cond_var_m.timed_wait(lock, t);
				break;
			}
		}

		if (!message_finished_m && response_finished_m) {
			response_finished_m = false;
		}
	}
	else {
		if (chunks_m.size() > 0) {
			*data = chunks_m.at(0);
			chunks_m.erase(chunks_m.begin());
			return true;
		}
		else {
			message_finished_m = true;

			if (caught_error_m) {
				caught_error_m = false;
				throw dealer_error(static_cast<cocaine::dealer::error_code>(resp_info_m.code), resp_info_m.error_msg);
			}

			return false;
		}
	}

	if (timeout >= 0.0 && chunks_m.empty() && !caught_error_m) {
		return false;
	}

	// expecting another chunk
	if (chunks_m.size() > 0) {
		*data = chunks_m.at(0);
		chunks_m.erase(chunks_m.begin());
		return true;
	}

	if (caught_error_m) {
		caught_error_m = false;
		throw dealer_error(static_cast<cocaine::dealer::error_code>(resp_info_m.code), resp_info_m.error_msg);
	}

	message_finished_m = true;
	return false;
}

void
response_impl::response_callback(const response_data& resp_data, const response_info& resp_info) {
	boost::mutex::scoped_lock lock(mutex_m);

	if (message_finished_m) {
		return;
	}

	if (resp_info.code == response_code::message_choke) {
		message_finished_m = true;
	}
	else if (resp_info.code == response_code::message_chunk) {
		chunks_m.push_back(new data_container(resp_data.data, resp_data.size));
	}
	else {
		caught_error_m = true;
		resp_info_m = resp_info; // remember error data
		message_finished_m = true;
	}

	response_finished_m = true;

	lock.unlock();
	cond_var_m.notify_one();
}

} // namespace dealer
} // namespace cocaine
