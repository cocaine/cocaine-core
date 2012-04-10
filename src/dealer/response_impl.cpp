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

namespace cocaine {
namespace dealer {

response_impl::response_impl(client* c) :
	client_(c),
	message_data_(NULL),
	message_size_(0),
	response_finished_(false),
	message_sent_(false),
	message_finished_(false)
{
	assert(c != NULL);
}

response_impl::~response_impl() {

}

void
response_impl::init(const void* data, size_t size, const message_path& path, const message_policy& policy) {
	message_data_ = data;
	message_size_ = size;
	message_path_ = path;
	message_policy_ = policy;
}

bool
response_impl::get() {
	boost::mutex::scoped_lock lock(mutex_);
	
	// no more chunks?
	if (message_finished_) {
		return false;
	}

	if (!message_sent_) {
		// send message
		std::cout << "sending message...\n";
		uuid_ = client_->get_impl()->send_message(message_data_, 
												  message_size_,
												  message_path_,
												  message_policy_);
		message_sent_ = true;

		// assign callback
		client_->set_response_callback(uuid_,
									   boost::bind(&response_impl::response_callback, this, _1, _2),
									   message_path_);
	}

	// block until received callback
	std::cout << "blocking...\n";

	while (!response_finished_) {
		cond_var_.wait(lock);
	}

	response_finished_ = false;

	std::cout << "done blocking.\n";

	// expecting another chunk
	if (resp_info_.code == response_code::message_chunk) {
		return true;
	}

	message_finished_ = true;
	return true;
}

void
response_impl::response_callback(const response_data& resp_data, const response_info& resp_info) {
	std::cout << "callback received.\n";

	boost::mutex::scoped_lock lock(mutex_);

	resp_info_ = resp_info;
	response_finished_ = true;

	lock.unlock();
	cond_var_.notify_one();
}

const void*
response_impl::data() {
	return NULL;
}

size_t
response_impl::size() {
	return 0;
}

int
response_impl::code() {
	return resp_info_.code;
}

std::string
response_impl::error_message() {
	return "";
}

} // namespace dealer
} // namespace cocaine
