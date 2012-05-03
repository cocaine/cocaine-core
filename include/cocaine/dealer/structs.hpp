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
