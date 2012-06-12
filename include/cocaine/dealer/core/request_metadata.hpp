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

#ifndef _COCAINE_DEALER_REQUEST_METADATA_HPP_INCLUDED_
#define _COCAINE_DEALER_REQUEST_METADATA_HPP_INCLUDED_

#include <string>
#include <sstream>

#include <boost/lexical_cast.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/message_path.hpp"
#include "cocaine/dealer/utils/time_value.hpp"
#include "cocaine/dealer/storage/eblob.hpp"
#include <boost/flyweight.hpp>

#include <msgpack.hpp>

namespace cocaine {
namespace dealer {

class request_metadata {
public:
	request_metadata() :
		data_size_(0),
		ack_received_(false),
		is_sent_(false),
		container_size_(false),
		retries_count_(0) {}

	virtual ~request_metadata() {}

	std::string as_string() const {
		std::stringstream s;
		s << std::boolalpha;
		s << "service: "<< path_.get().service_name << ", handle: ";
		s << path_.get().handle_name + "\n";
        s << "uuid: " << uuid_ << "\n";
        s << "policy [send to all hosts]: " << policy_.send_to_all_hosts << "\n";
        s << "policy [urgent]: " << policy_.urgent << "\n";
        s << "policy [mailboxed]: " << policy_.mailboxed << "\n";
        s << "policy [timeout]: " << policy_.timeout << "\n";
        s << "policy [deadline]: " << policy_.deadline << "\n";
        s << "policy [max retries]: " << policy_.max_retries << "\n";
        s << "data_size: " << data_size_ << "\n";
        s << "enqued timestamp: " << enqued_timestamp_.as_string();
        return s.str();
	}

	const message_path& path() const {
		return path_.get();
	}

	void set_path(const message_path& path) {
		path_ = path;
	}

	boost::flyweight<message_path> path_;
	message_policy policy_;
	std::string uuid_;
	std::string destination_endpoint_;
	uint64_t data_size_;

	time_value enqued_timestamp_;
	time_value sent_timestamp_;
	bool ack_received_;

	bool is_sent_;
	size_t container_size_;
	int retries_count_;
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

		msgpack::unpacker pac(size);
		memcpy(pac.buffer(), data, size);
		pac.buffer_consumed(size);

		message_path path;
		unpack_next_value(pac, path);
		path_ = path;

		unpack_next_value(pac, policy_);
		unpack_next_value(pac, uuid_);
		unpack_next_value(pac, data_size_);
		unpack_next_value(pac, enqued_timestamp_);
	}

	void commit_data() {
		// serialize all metadata
		msgpack::sbuffer buffer;
		msgpack::packer<msgpack::sbuffer> pk(&buffer);
    	pk.pack(path());
    	pk.pack(policy_);
    	pk.pack(uuid_);
    	pk.pack(data_size_);
    	pk.pack(enqued_timestamp_);

    	// write to eblob with uuid as key
		blob_.write(uuid_, buffer.data(), buffer.size(), EBLOB_COLUMN);
	}

private:
	template<typename T> void unpack_next_value(msgpack::unpacker& upack, T& value) {
	 	msgpack::unpacked result;
		upack.next(&result);
		result.get().convert(&value);
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
