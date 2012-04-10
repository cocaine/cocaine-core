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

#include <boost/current_function.hpp>

#include "cocaine/dealer/client.hpp"
#include "cocaine/dealer/response.hpp"
#include "cocaine/dealer/core/client_impl.hpp"

namespace cocaine {
namespace dealer {

client::client(const std::string& config_path) {
	impl_.reset(new client_impl(config_path));
	get_impl()->connect();
}

client::~client() {
}

void
client::set_response_callback(const std::string& message_uuid, response_callback callback, const message_path& path) {
	set_response_callback(message_uuid, callback, path);
}

response
client::send_message(const void* data, size_t size, const message_path& path, const message_policy& policy, bool discard_answer, bool block) {
	response r(this);
	r.init(data, size, path, policy);
	return r;
}

inline boost::shared_ptr<client_impl>
client::get_impl() {
	boost::mutex::scoped_lock lock(mutex_);

	if (impl_.get()) {
		return impl_;
	}
	else {
		throw error("client_impl object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}
}

} // namespace dealer
} // namespace cocaine
