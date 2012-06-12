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

#ifndef _COCAINE_DEALER_MESSAGE_IFACE_HPP_INCLUDED_
#define _COCAINE_DEALER_MESSAGE_IFACE_HPP_INCLUDED_

#include <string>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/utils/time_value.hpp"

namespace cocaine {
namespace dealer {

class message_iface {
public:
	virtual ~message_iface() {};

	virtual void* data() = 0;
	virtual size_t size() const = 0;

	virtual bool is_data_loaded() = 0;
	virtual void load_data() = 0;
	virtual void unload_data() = 0;

	virtual void remove_from_persistent_cache() = 0;

	virtual size_t container_size() const = 0;

	virtual const message_path& path() const = 0;
	virtual const message_policy& policy() const = 0;
	virtual const std::string& uuid() const = 0;

	virtual bool is_sent() const = 0;
	virtual const time_value& sent_timestamp() const = 0;
	virtual const time_value& enqued_timestamp() const = 0;

	virtual bool ack_received() const = 0;
	virtual void set_ack_received(bool value) = 0;

	virtual const std::string& destination_endpoint() const = 0;
	virtual void set_destination_endpoint(const std::string& value) = 0;

	virtual int retries_count() const = 0;
	virtual void increment_retries_count() = 0;
	virtual bool can_retry() const = 0;

	virtual void mark_as_sent(bool value) = 0;

	virtual std::string json() = 0;
	virtual bool is_expired() = 0;

	virtual message_iface& operator = (const message_iface& rhs) = 0;
	virtual bool operator == (const message_iface& rhs) const = 0;
	virtual bool operator != (const message_iface& rhs) const = 0;

	static const size_t MAX_MESSAGE_DATA_SIZE = 2147483648; // 2 gb
	static const size_t UUID_SIZE = 36; // bytes
	static const size_t ACK_TIMEOUT = 100; // millisecs
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_MESSAGE_IFACE_HPP_INCLUDED_
