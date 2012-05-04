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

#include <cocaine/dealer/response.hpp>
#include <cocaine/dealer/core/client_impl.hpp>
#include <cocaine/dealer/core/response_impl.hpp>

namespace cocaine {
namespace dealer {

response::response(const boost::shared_ptr<client_impl>& client, const std::string& uuid, const message_path& path) {
	impl_.reset(new response_impl(client, uuid, path));
}

response::~response() {
	impl_.reset();
}

bool
response::get(data_container* data, double timeout) {
	return get_impl()->get(data, timeout);
}

boost::shared_ptr<response_impl>
response::get_impl() {
	return impl_;
}

void
response::response_callback(const response_data& resp_data, const response_info& resp_info) {
	get_impl()->response_callback(resp_data, resp_info);
}

} // namespace dealer
} // namespace cocaine
