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
	std::cout << "resp is being killed\n";

	message_finished_ = true;
	response_finished_ = true;
	client_->unset_response_callback(uuid_, path_);

	std::cout << "resp killed\n";
}

bool
response_impl::get(data_container* data) {
	std::cout << "lock in GET - enter\n";
	boost::mutex::scoped_lock lock(mutex_);

	// no more chunks?
	if (message_finished_ && chunks_.size() == 0) {
		std::cout << "unlock in GET - finished\n";
		return false;
	}

	// block until received callback
	if (!message_finished_) {
		while (!response_finished_) {
			std::cout << "condvar BEGIN WAIT\n";
			cond_var_.wait(lock);
			std::cout << "condvar END WAIT\n";
		}

		response_finished_ = false;
	}
	else {
		if (chunks_.size() > 0) {
			*data = chunks_.at(0);
			chunks_.erase(chunks_.begin());
			std::cout << "unlock in GET - message finished, return chunk\n";
			return true;
		}
		else {
			message_finished_ = true;
			std::cout << "unlock in GET - message finished, no chunks\n";
			return false;
		}
	}

	// expecting another chunk
	if (chunks_.size() > 0) {
		//*data = chunks_.at(0);
		//chunks_.erase(chunks_.begin());
		lock.unlock();
		std::cout << "unlock in GET - message NOT finished, return chunk\n";
		return true;
	}

	message_finished_ = true;

	lock.unlock();
	std::cout << "unlock in GET - message finished, DONE\n";
	return false;
}

void
response_impl::response_callback(const response_data& resp_data, const response_info& resp_info) {
	std::cout << "lock in response_callback - enter, this: " << this << "\n";
	boost::mutex::scoped_lock lock(mutex_);

	if (message_finished_) {
		std::cout << "unlock in response_callback - FINISHED\n";
		return;
	}

	if (resp_info.code == response_code::message_choke) {
		std::cout << "CHOKE\n";
		message_finished_ = true;
	}
	else if (resp_info.code == response_code::message_chunk) {
		std::cout << "CHUNK\n";
		chunks_.push_back(new data_container(resp_data.data, resp_data.size));
	}
	else {
		// process errors
		std::cout << "ERROR\n";
		message_finished_ = true;
	}

	resp_info_ = resp_info;
	response_finished_ = true;

	std::cout << "unlock in response_callback\n";
	lock.unlock();

	std::cout << "notify_one in response_callback\n";
	cond_var_.notify_one();
}

} // namespace dealer
} // namespace cocaine
