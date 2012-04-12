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

response_impl::response_impl(client* c, const std::string& uuid, const message_path& path) :
	client_(c),
	uuid_(uuid),
	path_(path),
	response_finished_(false),
	message_finished_(false)
{
	assert(c != NULL);
}

response_impl::~response_impl() {
	//std::cout << "killing: " << uuid_ << std::endl;
	//client_->unset_response_callback(uuid_, path_);
	//message_finished_ = true;
	//response_finished_ = true;
}

bool
response_impl::get(data_container* data) {
	//std::cout << "get\n";

	boost::mutex::scoped_lock lock(mutex_);

	// no more chunks?
	if (message_finished_) {
		return false;
	}

	data_ = *data;

	// block until received callback
	while (!response_finished_) {
		cond_var_.wait(lock);
	}

	//response_finished_ = false;

	// expecting another chunk
	if (resp_info_.code == response_code::message_chunk) {
		lock.unlock();
		*data = data_;
		client_->unset_response_callback(uuid_, path_);
		return true;
	}

	//client_->unset_response_callback(uuid_, path_);
	message_finished_ = true;
	response_finished_ = true;

	throw error(resp_info_.code, resp_info_.error_msg);
	return false;
}

void
response_impl::response_callback(const response_data& resp_data, const response_info& resp_info) { 
	if (message_finished_) {
		return;
	}

	boost::mutex::scoped_lock lock(mutex_);

	if (resp_info.code == response_code::message_choke) {
		//std::cout << "choke\n";
		return;
	}
	else if (resp_info.code == response_code::message_chunk) {
		//std::cout << "chunk\n";
		data_.set_data(resp_data.data, resp_data.size);
	}
	else {
		//std::cout << "error!\n";
	}

	resp_info_ = resp_info;
	response_finished_ = true;
	message_finished_ = true;

	lock.unlock();
	cond_var_.notify_one();
}

} // namespace dealer
} // namespace cocaine
