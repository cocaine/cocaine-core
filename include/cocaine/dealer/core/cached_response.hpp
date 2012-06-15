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

#ifndef _COCAINE_DEALER_CACHED_RESPONSE_HPP_INCLUDED_
#define _COCAINE_DEALER_CACHED_RESPONSE_HPP_INCLUDED_

#include <string>
#include <sys/time.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/utils/data_container.hpp"

namespace cocaine {
namespace dealer {

class cached_response_t {
public:
	cached_response_t();
	explicit cached_response_t(const cached_response_t& response_t);

	cached_response_t(const std::string& uuid,
					  const std::string& route,
					  const message_path_t& path,
					  const void* data,
					  size_t data_size);

	cached_response_t(const std::string& uuid,
					  const std::string& route,
					  const message_path_t& path,
					  int code,
					  const std::string& error_message);

	virtual ~cached_response_t();

	const data_container& data() const;

	const message_path_t& path() const;
	const std::string& uuid() const;
	const std::string& route() const;

	int code() const;
	std::string error_message() const;

	void set_code(int code);
	void set_error_message(const std::string& message);

	const timeval& received_timestamp() const;
	void set_received_timestamp(const timeval& val);

	cached_response_t& operator = (const cached_response_t& rhs);
	bool operator == (const cached_response_t& rhs) const;
	bool operator != (const cached_response_t& rhs) const;

	static const size_t MAX_RESPONSE_DATA_SIZE = 2147483648; // 2gb
	static const size_t UUID_SIZE = 36; // bytes
	
private:
	std::string		m_uuid;
	std::string		m_route;
	message_path_t	m_path;
	data_container	m_data;
	timeval			m_received_timestamp;
	boost::mutex	m_mutex;

	int			m_code;
	std::string m_error_message;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CACHED_RESPONSE_HPP_INCLUDED_
