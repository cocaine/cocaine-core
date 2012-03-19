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

#ifndef _COCAINE_DEALER_REQUEST_METADATA_HPP_INCLUDED_
#define _COCAINE_DEALER_REQUEST_METADATA_HPP_INCLUDED_

#include <string>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/details/time_value.hpp"
#include "cocaine/dealer/details/eblob.hpp"

namespace cocaine {
namespace dealer {

class request_metadata {
public:
	request_metadata() {};
	virtual ~request_metadata() {};

	// metadata
	message_path path;
	message_policy policy;
	std::string uuid;
	time_value enqueued_timestamp;
};

class persistent_request_metadata : public request_metadata {
public:
	persistent_request_metadata() : request_metadata() {};
	virtual ~persistent_request_metadata() {};

	void set_eblob(eblob blob) {
		blob_ = blob;
	}

	static const size_t EBLOB_COLUMN = 0;

	void commit_data() {
		// serialize to eblob with uuid
		msgpack::sbuffer buffer;
		msgpack::pack(buffer, path.service_name);
		msgpack::pack(buffer, path.handle_name);
		msgpack::pack(buffer, policy.send_to_all_hosts);
		msgpack::pack(buffer, policy.urgent);
		msgpack::pack(buffer, policy.mailboxed);
		msgpack::pack(buffer, policy.timeout);
		msgpack::pack(buffer, policy.deadline);
		msgpack::pack(buffer, policy.max_timeout_retries);
		msgpack::pack(buffer, uuid);
		msgpack::pack(buffer, enqueued_timestamp.as_timeval().tv_sec);
		msgpack::pack(buffer, enqueued_timestamp.as_timeval().tv_usec);
		
		blob_.write(uuid, buffer.data(), buffer.size(), EBLOB_COLUMN);

				/*
		//lsd::time_value tv;
		tv.init_from_current_time();

		// create message policy
		lsd::message_policy policy;
		policy.deadline = tv.as_double() + 10.0;

		// create message data
		std::map<std::string, int> event;
		event["service"] = 1;
		event["uid"] = 12345;
		event["score"] = 500;

		msgpack::sbuffer buffer;
		msgpack::pack(buffer, event);

		std::string message = "whoa!";

		// send messages
		for (int i = 0; i < add_messages_count; ++i) {
			std::string uuid1 = c.send_message(message, path);
			std::cout << "mesg uuid: " << uuid1 << std::endl;
		}
		*/

		//msgpack::unpacked msg;
		//msgpack::unpack(&msg, (const char*)response.data, response.size);
		//msgpack::object obj = msg.get();
		//std::stringstream stream;

	}

private:
	eblob blob_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_REQUEST_METADATA_HPP_INCLUDED_
