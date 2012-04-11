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
#include <cocaine/dealer/client.hpp>
#include <cocaine/dealer/core/response_impl.hpp>

namespace cocaine {
namespace dealer {

response::response(client* c, const std::string& uuid, const message_path& path) {
	impl_.reset(new response_impl(c, uuid, path));
}

response::~response() {
	impl_.reset();
}

bool
response::get(data_container* data) {
	return get_impl()->get(data);
}

const void*
response::data() {
	return get_impl()->data();
}

size_t
response::size() {
	return get_impl()->size();
}

int
response::code() {
	return get_impl()->code();
}

std::string
response::error_message() {
	return get_impl()->error_message();
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
