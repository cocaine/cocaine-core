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
#include <boost/cstdint.hpp>

namespace cocaine {
namespace dealer {

static const int PROTOCOL_VERSION = 1;
static const int STATISTICS_PROTOCOL_VERSION = 1;
static const unsigned long long MESSAGE_TIMEOUT = 10;	// seconds
static const unsigned long long HEARTBEAT_INTERVAL = 1;	// seconds
static const unsigned long long DEFAULT_SOCKET_POLL_TIMEOUT = 2000; // milliseconds
static const unsigned long long DEFAULT_SOCKET_PING_TIMEOUT = 1000; // milliseconds

static const std::string DEFAULT_EBLOB_PATH = "/tmp/pmq_eblob";
static int64_t DEFAULT_EBLOB_BLOB_SIZE = 2147483648; // 2 gb
static const int DEFAULT_EBLOB_SYNC_INTERVAL = 1;

static const std::string DEFAULT_HOSTS_URL = "";
static const unsigned short DEFAULT_CONTROL_PORT = 5555;
static const std::string DEFAULT_MULTICAST_IP = "226.1.1.1";
static const unsigned short DEFAULT_MULTICAST_PORT = 5556;
static const unsigned short DEFAULT_STATISTICS_PORT = 3333;
static const size_t DEFAULT_MAX_MESSAGE_CACHE_SIZE = 512; // megabytes

struct dealer_types {
	typedef boost::uint32_t ip_addr;
	typedef boost::uint16_t port;
};

// main types definition
typedef dealer_types DT;

enum callback_error {
	UNKNOWN_SERVICE_ERROR = 1,
	CALLBACK_EXISTS_ERROR = 2
};

enum response_error {
	MESSAGE_CHUNK = 1,
	MESSAGE_CHOKE = 2,
	EXPIRED_MESSAGE_ERROR = 520,
	MESSAGE_QUEUE_IS_FULL = 503
};

enum logger_type {
	STDOUT_LOGGER = 1,
	FILE_LOGGER,
	SYSLOG_LOGGER
};

enum autodiscovery_type {
	AT_MULTICAST = 1,
	AT_HTTP
};

enum message_cache_type {
	RAM_ONLY = 1,
	PERSISTANT
};

struct message_path {
	message_path() {};
	message_path(const std::string& service_name_,
				 const std::string& handle_name_) :
		service_name(service_name_),
		handle_name(handle_name_) {};

	message_path(const message_path& path) :
		service_name(path.service_name),
		handle_name(path.handle_name) {};

	message_path& operator = (const message_path& rhs) {
		if (this == &rhs) {
			return *this;
		}

		service_name = rhs.service_name;
		handle_name = rhs.handle_name;

		return *this;
	}

	bool operator == (const message_path& mp) const {
		return (service_name == mp.service_name &&
				handle_name == mp.handle_name);
	}

	bool operator != (const message_path& mp) const {
		return !(*this == mp);
	}

	size_t data_size() const {
		return (service_name.length() + handle_name.length());
	}

	std::string service_name;
	std::string handle_name;
};

struct message_policy {
	message_policy() :
		send_to_all_hosts(false),
		urgent(false),
		mailboxed(false),
		timeout(0.0f),
		deadline(0.0f),
		max_timeout_retries(0) {};

	message_policy(bool send_to_all_hosts_,
				   bool urgent_,
				   float mailboxed_,
				   float timeout_,
				   float deadline_,
				   int max_timeout_retries_) :
		send_to_all_hosts(send_to_all_hosts_),
		urgent(urgent_),
		mailboxed(mailboxed_),
		timeout(timeout_),
		deadline(deadline_),
		max_timeout_retries(max_timeout_retries_) {};

	message_policy(const message_policy& mp) {
		*this = mp;
	}

	message_policy& operator = (const message_policy& rhs) {
		if (this == &rhs) {
			return *this;
		}

		send_to_all_hosts = rhs.send_to_all_hosts;
		urgent = rhs.urgent;
		mailboxed = rhs.mailboxed;
		timeout = rhs.timeout;
		deadline = rhs.deadline;
		max_timeout_retries = rhs.max_timeout_retries;

		return *this;
	}

	bool operator == (const message_policy& rhs) const {
		return (send_to_all_hosts == rhs.send_to_all_hosts &&
				urgent == rhs.urgent &&
				mailboxed == rhs.mailboxed &&
				timeout == rhs.timeout &&
				deadline == rhs.deadline);
	}

	bool operator != (const message_policy& rhs) const {
		return !(*this == rhs);
	}

	bool send_to_all_hosts;
    bool urgent;
    bool mailboxed;
    double timeout;
    double deadline;
    int max_timeout_retries;
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

struct response {
	response() : data(NULL), size(0) {};
	std::string uuid;
	void* data;
	size_t size;
};

struct response_info {
	response_info() : error(0) {};
	std::string service;
	std::string handle;
	int error;
	std::string error_msg;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_STRUCTS_HPP_INCLUDED_
