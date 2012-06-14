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

#ifndef _COCAINE_DEALER_STRUCTS_HPP_INCLUDED_
#define _COCAINE_DEALER_STRUCTS_HPP_INCLUDED_

#include <vector>
#include <map>
#include <stdexcept>

#include <time.h>

#include <msgpack.hpp>

#include <boost/cstdint.hpp>

#include <cocaine/dealer/types.hpp>
#include <cocaine/dealer/message_path.hpp>
#include <cocaine/dealer/message_policy.hpp>

namespace cocaine {
namespace dealer {

struct dealer_types {
	typedef boost::uint32_t ip_addr;
	typedef boost::uint16_t port;
};

// main types definition
typedef dealer_types DT;

struct response_code {
	static const int unknown_error = 1;
	static const int message_chunk = 2;
	static const int message_choke = 3;
};

struct msg_queue_status {
	msg_queue_status() :
		pending(0),
		sent(0) {};

	// amount of messages currently queued
	size_t pending;

	// amount of sent messages that haven't yed received response from server
	size_t sent;
};

struct handle_stats {
	handle_stats() :
		sent_messages(0),
		resent_messages(0),
		bad_sent_messages(0),
		all_responces(0),
		normal_responces(0),
		timedout_responces(0),
		err_responces(0),
		expired_responses(0) {};

	// tatal sent msgs (with resent msgs)
	size_t sent_messages;

	// timeout or queue full msgs
	size_t resent_messages;

	// messages failed during sending
	size_t bad_sent_messages;

	// all responces received count
	size_t all_responces;

	// successful responces (no errs)
	size_t normal_responces;

	// responces with timedout msgs
	size_t timedout_responces;

	// error responces (deadline met, failed code parsing, app err, etc.)
	size_t err_responces;

	// expired messages
	size_t expired_responses;

	// handle queue status
	struct msg_queue_status queue_status;
};

struct service_stats {
	// <ip address, hostname>
	std::map<DT::ip_addr, std::string> hosts;

	// <handle name>
	std::vector<std::string> handles;

	// <handle name, queue_size>
	std::map<std::string, size_t> unhandled_messages;
};

struct response_data {
	response_data() : data(NULL), size(0) {};
	void* data;
	size_t size;
};

struct response_info {
	response_info() : code(0) {};
	std::string uuid;
	message_path path;
	int code;
	std::string error_msg;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_STRUCTS_HPP_INCLUDED_
