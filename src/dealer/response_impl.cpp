//
// Copyright (C) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#include <stdexcept>
#include <iostream>

#include <boost/current_function.hpp>
#include <boost/bind.hpp>

#include "cocaine/dealer/client.hpp"
#include <cocaine/dealer/core/response_impl.hpp>
#include <cocaine/dealer/core/client_impl.hpp>
#include <cocaine/dealer/utils/error.hpp>

namespace cocaine {
namespace dealer {

response_impl::response_impl(const boost::shared_ptr<client_impl>& client_ptr, const std::string& uuid, const message_path& path) :
	client_(client_ptr),
	uuid_(uuid),
	path_(path),
	response_finished_(false),
	message_finished_(false),
	caught_error_(false)
{
	assert(client_ptr.get() != NULL);
}

response_impl::~response_impl() {
	boost::mutex::scoped_lock lock(mutex_);
	message_finished_ = true;
	response_finished_ = true;
	chunks_.clear();

	boost::shared_ptr<client_impl> client_ptr = client_.lock();
	if (client_ptr) {
		client_ptr->unset_response_callback(uuid_, path_);
	}	
}

bool
response_impl::get(data_container* data) {
	boost::mutex::scoped_lock lock(mutex_);

	// no more chunks?
	if (message_finished_ && chunks_.size() == 0) {
		if (!caught_error_) {
			return false;
		}
	}

	// block until received callback
	if (!message_finished_) {
		while (!response_finished_ && !message_finished_) {
			cond_var_.wait(lock);
		}

		if (!message_finished_) {
			response_finished_ = false;
		}
	}
	else {
		if (chunks_.size() > 0) {
			*data = chunks_.at(0);
			chunks_.erase(chunks_.begin());
			return true;
		}
		else {
			message_finished_ = true;

			if (caught_error_) {
				caught_error_ = false;
				throw dealer_error(static_cast<cocaine::dealer::error_code>(resp_info_.code), resp_info_.error_msg);
			}

			return false;
		}
	}

	// expecting another chunk
	if (chunks_.size() > 0) {
		*data = chunks_.at(0);
		chunks_.erase(chunks_.begin());
		return true;
	}

	if (caught_error_) {
		caught_error_ = false;
		throw dealer_error(static_cast<cocaine::dealer::error_code>(resp_info_.code), resp_info_.error_msg);
	}

	message_finished_ = true;
	return false;
}

void
response_impl::response_callback(const response_data& resp_data, const response_info& resp_info) {
	boost::mutex::scoped_lock lock(mutex_);

	if (message_finished_) {
		return;
	}

	if (resp_info.code == response_code::message_choke) {
		message_finished_ = true;
	}
	else if (resp_info.code == response_code::message_chunk) {
		chunks_.push_back(new data_container(resp_data.data, resp_data.size));
	}
	else {
		caught_error_ = true;
		resp_info_ = resp_info; // remember error data
		message_finished_ = true;
	}

	response_finished_ = true;

	lock.unlock();
	cond_var_.notify_one();
}

} // namespace dealer
} // namespace cocaine
