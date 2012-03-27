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
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/utils/time_value.hpp"
#include "cocaine/dealer/storage/eblob.hpp"

#include <msgpack.hpp>

namespace cocaine {
namespace dealer {

class request_metadata {
public:
	request_metadata() {};
	virtual ~request_metadata() {};

	std::string as_string() const {
		std::stringstream s;
		s << std::boolalpha;
		s << "service/handle: " << path.service_name << "/" << path.handle_name << "\n";
        s << "uuid: " << uuid << "\n";
        s << "policy [send to all hosts]: " << policy.send_to_all_hosts << "\n";
        s << "policy [urgent]: " << policy.urgent << "\n";
        s << "policy [mailboxed]: " << policy.mailboxed << "\n";
        s << "policy [timeout]: " << policy.timeout << "\n";
        s << "policy [deadline]: " << policy.deadline << "\n";
        s << "policy [max timeout retries]: " << policy.max_timeout_retries << "\n";
        s << "data_size: " << data_size << "\n";
        s << "enqueued timestamp: " << enqueued_timestamp.as_string();
        return s.str();
	}

	// metadata
	message_path path;
	message_policy policy;
	std::string uuid;
	uint64_t data_size;
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

	void load_data(void* data, size_t size) {
		if (!data || size == 0) {
			return;
		}

		msgpack::unpacked msg;
		msgpack::unpack(&msg, reinterpret_cast<char*>(data), size);
		msgpack::object obj = msg.get();
		obj.convert(this);
	}

	void commit_data() {
		// serialize to eblob with uuid as key
		msgpack::sbuffer buffer;
		msgpack::pack(buffer, *this);
		blob_.write(uuid, buffer.data(), buffer.size(), EBLOB_COLUMN);
	}

	MSGPACK_DEFINE(path, policy, uuid, data_size, enqueued_timestamp);

private:
	template<typename T> void unpack_next_value(msgpack::unpacker& upack, T& value) {
		msgpack::unpacked result;
		msgpack::object obj;

        upack.next(&result);
		obj = result.get();
        obj.convert(&value);
	}

private:
	eblob blob_;
};

std::ostream& operator << (std::ostream& out, request_metadata& req_meta) {
	out << req_meta.as_string();
	return out;
}

std::ostream& operator << (std::ostream& out, persistent_request_metadata& p_req_meta) {
	out << p_req_meta.as_string();
	return out;
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_REQUEST_METADATA_HPP_INCLUDED_
