//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
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
#include "cocaine/dealer/details/client_impl.hpp"

namespace lsd {

client::client(const std::string& config_path) {
	impl_.reset(new client_impl(config_path));
}

client::~client() {
}

void
client::connect() {
	get_impl()->connect();
}

void
client::disconnect() {
	get_impl()->disconnect();
}

std::string
client::send_message(const void* data,
					 size_t size,
					 const std::string& service_name,
					 const std::string& handle_name)
{
	return get_impl()->send_message(data, size, service_name, handle_name);
}

std::string
client::send_message(const void* data,
					 size_t size,
					 const message_path& path)
{
	return get_impl()->send_message(data, size, path);
}

std::string
client::send_message(const void* data,
					 size_t size,
					 const message_path& path,
					 const message_policy& policy)
{
	return get_impl()->send_message(data, size, path, policy);
}

std::string
client::send_message(const std::string& data,
					 const std::string& service_name,
					 const std::string& handle_name)
{
	return get_impl()->send_message(data, service_name, handle_name);
}

std::string
client::send_message(const std::string& data,
					 const message_path& path)
{
	return get_impl()->send_message(data, path);
}

std::string
client::send_message(const std::string& data,
					 const message_path& path,
					 const message_policy& policy)
{
	return get_impl()->send_message(data, path, policy);
}

int
client::set_response_callback(boost::function<void(const response&, const response_info&)> callback,
						   	  const std::string& service_name,
						   	  const std::string& handle_name)
{
	return get_impl()->set_response_callback(callback, service_name, handle_name);
}

int
client::set_response_callback(boost::function<void(const response&, const response_info&)> callback,
							  const message_path& path)
{
	return set_response_callback(callback, path.service_name, path.handle_name);
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

} // namespace lsd
