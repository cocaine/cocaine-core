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
