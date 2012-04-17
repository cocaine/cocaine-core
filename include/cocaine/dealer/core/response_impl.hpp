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
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>

#include <cocaine/dealer/forwards.hpp>
#include <cocaine/dealer/structs.hpp>
#include <cocaine/dealer/utils/data_container.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace cocaine {
namespace dealer {

class response_impl {
public:
	response_impl(const boost::shared_ptr<client_impl>& client, const std::string& uuid, const message_path& path);
	~response_impl();

	bool get(data_container* data);

private:
	friend class response;

	void response_callback(const response_data& resp_data, const response_info& resp_info);

	boost::ptr_vector<data_container> chunks_;

	boost::weak_ptr<client_impl> client_;
	std::string uuid_;
	const message_path path_;
	bool response_finished_;
	bool message_finished_;

	response_info resp_info_;

	boost::mutex mutex_;
	boost::condition_variable cond_var_;

	bool caught_error_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_RESPONSE_IMPL_HPP_INCLUDED_
