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

#ifndef _COCAINE_DEALER_CLIENT_HPP_INCLUDED_
#define _COCAINE_DEALER_CLIENT_HPP_INCLUDED_

#include <string>

#include <boost/utility.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include <cocaine/dealer/forwards.hpp>
#include <cocaine/dealer/structs.hpp>
#include <cocaine/dealer/response.hpp>

namespace cocaine {
namespace dealer {

class dealer_t : private boost::noncopyable {
public:
	typedef boost::function<void(const response_data&, const response_info&)> response_callback;

	explicit dealer_t(const std::string& config_path = "");
	virtual ~dealer_t();

	boost::shared_ptr<response> send_message(const void* data,
											 size_t size,
											 const message_path& path,
											 const message_policy& policy);

	template <typename T> boost::shared_ptr<response>
								send_message(const T& object,
											 const message_path& path,
											 const message_policy& policy) {
		msgpack::sbuffer buffer;
		msgpack::pack(buffer, object);
		return send_message(reinterpret_cast<const void*>(buffer.data()), buffer.size(), path, policy);
	}

private:
	friend class response_impl;

	boost::shared_ptr<dealer_impl_t> get_impl();

	boost::shared_ptr<dealer_impl_t> impl_;
	mutable boost::mutex mutex_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CLIENT_HPP_INCLUDED_
