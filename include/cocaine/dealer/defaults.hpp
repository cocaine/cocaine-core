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

#ifndef _COCAINE_DEALER_DEFAULTS_HPP_INCLUDED_
#define _COCAINE_DEALER_DEFAULTS_HPP_INCLUDED_

#include <vector>
#include <map>
#include <stdexcept>

#include <time.h>

#include <msgpack.hpp>

#include <boost/cstdint.hpp>

#include <cocaine/dealer/types.hpp>
#include <cocaine/dealer/message_path.hpp>
#include <cocaine/dealer/message_policy.hpp>

#include <cocaine/dealer/utils/smart_logger.hpp>

namespace cocaine {
namespace dealer {

enum e_autodiscovery_type {
	AT_MULTICAST = 1,
	AT_HTTP,
	AT_FILE
};

enum e_message_cache_type {
	RAM_ONLY = 1,
	PERSISTENT
};

struct defaults {
	// logger
	static const enum e_logger_type logger_type = STDOUT_LOGGER;
	static const unsigned int logger_flags = PLOG_NONE;

	// autodiscovery
	static const enum e_autodiscovery_type autodiscovery_type = AT_FILE;

	// persistance
	static const enum e_message_cache_type message_cache_type = RAM_ONLY;

	// the rest
	static const int protocol_version = 1;
	static const unsigned long long message_deadline = 500;	// milliseconds
	static const unsigned long long socket_poll_timeout = 2000; // milliseconds
	static const unsigned long long socket_ping_timeout = 1000; // milliseconds

	static const std::string eblob_path;
	static const size_t eblob_blob_size = 2147483648; // 2 gb
	static const int eblob_sync_interval = 2;

	static const unsigned short control_port = 5000;
	static const unsigned long long heartbeat_interval = 2;	// seconds
	static const std::string heartbeat_multicast_ip;
	static const unsigned short heartbeat_multicast_port = 5556;

	static const unsigned short statistics_port = 3333;
	static const int statistics_protocol_version = 1;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_DEFAULTS_HPP_INCLUDED_
