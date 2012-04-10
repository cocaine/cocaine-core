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

#ifndef _COCAINE_DEALER_RESPONSE_IMPL_HPP_INCLUDED_
#define _COCAINE_DEALER_RESPONSE_IMPL_HPP_INCLUDED_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <cocaine/dealer/forwards.hpp>
#include <cocaine/dealer/structs.hpp>

namespace cocaine {
namespace dealer {

class response_impl {
public:
	explicit response_impl(client* c);
	virtual ~response_impl();

	bool get();
	const void* data();
	size_t size();
	int code();
	std::string error_message();

private:
	friend class response;

	void init(const void* data, size_t size, const message_path& path, const message_policy& policy);
	void response_callback(const response_data& resp_data, const response_info& resp_info);

	client* client_;

	const void* message_data_;
	size_t message_size_;
	message_path message_path_;
	message_policy message_policy_;
	std::string uuid_;
	bool response_finished_;
	bool message_finished_;
	bool message_sent_;

	response_info resp_info_;

	boost::mutex mutex_;
	boost::condition_variable cond_var_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_RESPONSE_IMPL_HPP_INCLUDED_
