/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

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
	response_impl(const boost::shared_ptr<dealer_impl_t>& dealer, const std::string& uuid, const message_path& path);
	~response_impl();

	// 1) timeout < 0 - block indefinitely until response received
	// 2) timeout == 0 - check for response chunk and return result immediately
	// 3) timeout > 0 - check for response chunk with some timeout value

	bool get(data_container* data, double timeout);

private:
	friend class response;

	void response_callback(const response_data& resp_data, const response_info& resp_info);

	boost::ptr_vector<data_container> chunks_;

	boost::weak_ptr<dealer_impl_t> client_;
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
