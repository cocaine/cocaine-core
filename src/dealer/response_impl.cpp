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

response_impl::response_impl(const boost::shared_ptr<client_impl>& client, const std::string& uuid, const message_path& path) :
	client_(client),
	uuid_(uuid),
	path_(path),
	response_finished_(false),
	message_finished_(false)
{
	assert(client.get() != NULL);
}

response_impl::~response_impl() {
	message_finished_ = true;
	response_finished_ = true;
	client_->unset_response_callback(uuid_, path_);
}

bool
response_impl::get(data_container* data) {
	//std::cout << "response_impl::get\n";
	//std::cout << "get\n";

	boost::mutex::scoped_lock lock(mutex_);

	// no more chunks?
	if (message_finished_ && chunks_.size() == 0) {
		return false;
	}

	// block until received callback
	if (!message_finished_) {
		while (!response_finished_) {
			cond_var_.wait(lock);
		}

		response_finished_ = false;
	}
	else {
		if (chunks_.size() > 0) {
			*data = chunks_.at(0);
			chunks_.erase(chunks_.begin());
			return true;
		}
		else {
			message_finished_ = true;
			return false;
		}
	}

	// expecting another chunk
	if (chunks_.size() > 0) {
		*data = chunks_.at(0);
		chunks_.erase(chunks_.begin());

		return true;
	}

	/*
	if (resp_info_.code == response_code::message_choke) {
		client_->unset_response_callback(uuid_, path_);
		return false;
	}

	//client_->unset_response_callback(uuid_, path_);
	message_finished_ = true;
	response_finished_ = true;

	throw dealer_error(static_cast<enum error_code>(resp_info_.code), resp_info_.error_msg);
	*/

	return false;
}

void
response_impl::response_callback(const response_data& resp_data, const response_info& resp_info) {
	//std::cout << "response_impl::response_callback!\n";

	boost::mutex::scoped_lock lock(mutex_);

	if (message_finished_) {
		return;
	}

	if (resp_info.code == response_code::message_choke) {
		//std::cout << "choke\n";
		message_finished_ = true;
	}
	else if (resp_info.code == response_code::message_chunk) {
		//std::cout << "chunk\n";
		chunks_.push_back(new data_container(resp_data.data, resp_data.size));
	}
	else {
		std::cout << "error!\n";
	}

	resp_info_ = resp_info;
	response_finished_ = true;
	lock.unlock();
	cond_var_.notify_one();
}

} // namespace dealer
} // namespace cocaine
