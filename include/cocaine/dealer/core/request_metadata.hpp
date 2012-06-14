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
#include <boost/shared_ptr.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/message_path.hpp"
#include "cocaine/dealer/utils/time_value.hpp"
#include "cocaine/dealer/storage/eblob.hpp"
#include <boost/flyweight.hpp>

#include <msgpack.hpp>

namespace cocaine {
namespace dealer {

struct request_metadata {
	request_metadata() :
		data_size(0),
		ack_received(false),
		is_sent(false),
		retries_count(0) {}

	virtual ~request_metadata() {}

	std::string as_string() const {
		std::stringstream s;
		s << std::boolalpha;
		s << "service: "<< path().service_alias << ", handle: ";
		s << path().handle_name + "\n";
        s << "uuid: " << uuid << "\n";
        s << "policy [send to all hosts]: " << policy.send_to_all_hosts << "\n";
        s << "policy [urgent]: " << policy.urgent << "\n";
        s << "policy [timeout]: " << policy.timeout << "\n";
        s << "policy [deadline]: " << policy.deadline << "\n";
        s << "policy [max retries]: " << policy.max_retries << "\n";
        s << "data_size: " << data_size << "\n";
        s << "enqued timestamp: " << enqued_timestamp.as_string();
        return s.str();
	}

	const message_path& path() const {
		return path_.get();
	}

	void set_path(const message_path& path) {
		path_ = path;
	}

	std::string		uuid;
	message_policy	policy;
	std::string		destination_endpoint;
	uint64_t		data_size;

	time_value	enqued_timestamp;
	time_value	sent_timestamp;
	bool		ack_received;

	bool	is_sent;
	int		retries_count;

private:
	boost::flyweight<message_path> path_;
};

struct persistent_request_metadata : public request_metadata {
	persistent_request_metadata() :
		request_metadata() {}

	virtual ~persistent_request_metadata() {}

	void set_eblob(const boost::shared_ptr<eblob>& blob_) {
		blob = blob_;
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
		path = path;

		unpack_next_value(pac, policy);
		unpack_next_value(pac, uuid);
		unpack_next_value(pac, data_size);
		unpack_next_value(pac, enqued_timestamp);
	}

	void commit_data() {
		// serialize all metadata
		msgpack::sbuffer buffer;
		msgpack::packer<msgpack::sbuffer> pk(&buffer);
    	pk.pack(path());
    	pk.pack(policy);
    	pk.pack(uuid);
    	pk.pack(data_size);
    	pk.pack(enqued_timestamp);

    	// write to eblob with uuid as key
		blob->write(uuid, buffer.data(), buffer.size(), EBLOB_COLUMN);
	}

private:
	template<typename T> void unpack_next_value(msgpack::unpacker& upack, T& value) {
	 	msgpack::unpacked result;
		upack.next(&result);
		result.get().convert(&value);
	}

	boost::shared_ptr<eblob> blob;
};

std::ostream& operator << (std::ostream& out, request_metadata& req_meta) {
	out << req_meta.as_string();
	return out;
}

std::ostream& operator << (std::ostream& out, persistent_request_metadata& req_meta) {
	out << req_meta.as_string();
	return out;
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_REQUEST_METADATA_HPP_INCLUDED_
