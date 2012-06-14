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

#ifndef _COCAINE_DEALER_CACHED_MESSAGE_HPP_INCLUDED_
#define _COCAINE_DEALER_CACHED_MESSAGE_HPP_INCLUDED_

#include <string>
#include <sys/time.h>
#include <cstring>
#include <iomanip>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>
#include "json/json.h"

#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/uuid.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"

namespace cocaine {
namespace dealer {

template<typename DataContainer, typename MetadataContainer>
class cached_message_t : public message_iface {
public:
	cached_message_t();
	explicit cached_message_t(const cached_message_t& message);

	cached_message_t(const message_path& path,
					 const message_policy& policy,
					 const void* data,
					 size_t data_size);

	cached_message_t(void* mdata,
					 size_t mdata_size);

	~cached_message_t();

	void* data();
	size_t size() const;

	DataContainer& data_container();
	MetadataContainer& mdata_container();

	const message_path& path() const;
	const message_policy& policy() const;
	const std::string& uuid() const;

	bool is_sent() const;
	const time_value& sent_timestamp() const;
	const time_value& enqued_timestamp() const;

	bool ack_received() const;
	void set_ack_received(bool value);

	const std::string& destination_endpoint() const;
	void set_destination_endpoint(const std::string& value);

	void mark_as_sent(bool value);

	bool is_expired();

	message_iface& operator = (const message_iface& rhs);
	bool operator == (const message_iface& rhs) const;
	bool operator != (const message_iface& rhs) const;

	bool is_data_loaded();
	void load_data();
	void unload_data();

	int retries_count() const;
	void increment_retries_count();
	bool can_retry() const;

	void remove_from_persistent_cache();

private:
	void init();
	
private:
	DataContainer		data_m;
	MetadataContainer	metadata_m;

	boost::mutex mutex_m;
};

template<typename DataContainer, typename MetadataContainer>
cached_message_t<DataContainer, MetadataContainer>::cached_message_t() {
	init();
}

template<typename DataContainer, typename MetadataContainer>
cached_message_t<DataContainer, MetadataContainer>::cached_message_t(const cached_message_t& message) {
	*this = message;
}

template<typename DataContainer, typename MetadataContainer>
cached_message_t<DataContainer, MetadataContainer>::cached_message_t(const message_path& path,
							   										 const message_policy& policy,
							   										 const void* data,
							   										 size_t data_size)
{
	metadata_m.set_path(path);
	metadata_m.policy = policy;
	metadata_m.enqued_timestamp.init_from_current_time();

	if (data_size > MAX_MESSAGE_DATA_SIZE) {
		throw dealer_error(resource_error, "can't create message, message data too big.");
	}

	data_m.set_data(data, data_size);
	init();
}

template<typename DataContainer, typename MetadataContainer>
cached_message_t<DataContainer, MetadataContainer>::cached_message_t(void* mdata, size_t mdata_size) {
	metadata_m.load_data(metadata_m, mdata_size);
}

template<typename DataContainer, typename MetadataContainer>
cached_message_t<DataContainer, MetadataContainer>::~cached_message_t() {
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::init() {
	metadata_m.uuid = wuuid_t().generate();
	metadata_m.enqued_timestamp.init_from_current_time();
}

template<typename DataContainer, typename MetadataContainer> int
cached_message_t<DataContainer, MetadataContainer>::retries_count() const {
	return metadata_m.retries_count;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::increment_retries_count() {
	++metadata_m.retries_count;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::can_retry() const {
	if (metadata_m.policy.max_retries < 0) {
		return true;
	}

	return (metadata_m.retries_count < metadata_m.policy.max_retries ? true : false);
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::is_data_loaded() {
	return data_m.is_data_loaded();
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::load_data() {
	data_m.load_data();
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::unload_data() {
	data_m.unload_data();
}

template<typename DataContainer, typename MetadataContainer> void*
cached_message_t<DataContainer, MetadataContainer>::data() {
	return data_m.data();
}

template<typename DataContainer, typename MetadataContainer> size_t
cached_message_t<DataContainer, MetadataContainer>::size() const {
	return data_m.size();
}

template<typename DataContainer, typename MetadataContainer> DataContainer&
cached_message_t<DataContainer, MetadataContainer>::data_container() {
	return data_m;
}

template<typename DataContainer, typename MetadataContainer> MetadataContainer&
cached_message_t<DataContainer, MetadataContainer>::mdata_container() {
	return metadata_m;
}

template<typename DataContainer, typename MetadataContainer> message_iface&
cached_message_t<DataContainer, MetadataContainer>::operator = (const message_iface& rhs) {
	boost::mutex::scoped_lock lock(mutex_m);

	if (this == &rhs) {
		return *this;
	}

	try {
		const cached_message_t<DataContainer, MetadataContainer>& dc = dynamic_cast<const cached_message_t<DataContainer, MetadataContainer>& >(rhs);

		data_m = dc.data_m;
		metadata_m = dc.metadata_m;
	}
	catch (const std::exception& ex) {
		std::string error_msg = ex.what();
		error_msg = "error in cached_message_t<DataContainer, MetadataContainer>::operator = (), details: " + error_msg;
		throw internal_error(error_msg);
	}

	return *this;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::operator == (const message_iface& rhs) const {

	bool comparison_result = false;

	try {
		const cached_message_t<DataContainer, MetadataContainer>& dc = dynamic_cast<const cached_message_t<DataContainer, MetadataContainer>&>(rhs);
		comparison_result = (metadata_m.uuid == dc.metadata_m.uuid);
	}
	catch (const std::exception& ex) {
		std::string error_msg = ex.what();
		error_msg = "error in cached_message_t<DataContainer, MetadataContainer>::operator = (), details: " + error_msg;
		throw internal_error(error_msg);
	}

	return comparison_result;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::operator != (const message_iface& rhs) const {
	return !(*this == rhs);
}

template<typename DataContainer, typename MetadataContainer> const std::string&
cached_message_t<DataContainer, MetadataContainer>::uuid() const {
	return metadata_m.uuid;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::is_sent() const {
	return metadata_m.is_sent;
}

template<typename DataContainer, typename MetadataContainer> const time_value&
cached_message_t<DataContainer, MetadataContainer>::sent_timestamp() const {
	return metadata_m.sent_timestamp;
}

template<typename DataContainer, typename MetadataContainer> const time_value&
cached_message_t<DataContainer, MetadataContainer>::enqued_timestamp() const {
	return metadata_m.enqued_timestamp;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::ack_received() const {
	return metadata_m.ack_received;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::set_ack_received(bool value) {
	metadata_m.ack_received = value;
}

template<typename DataContainer, typename MetadataContainer> const std::string&
cached_message_t<DataContainer, MetadataContainer>::destination_endpoint() const {
	return metadata_m.destination_endpoint;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::set_destination_endpoint(const std::string& value) {
	metadata_m.destination_endpoint = value;	
}

template<typename DataContainer, typename MetadataContainer> const message_path&
cached_message_t<DataContainer, MetadataContainer>::path() const {
	return metadata_m.path();
}

template<typename DataContainer, typename MetadataContainer> const message_policy&
cached_message_t<DataContainer, MetadataContainer>::policy() const {
	return metadata_m.policy;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::mark_as_sent(bool value) {
	boost::mutex::scoped_lock lock(mutex_m);

	if (value) {
		metadata_m.is_sent = true;
		metadata_m.sent_timestamp.init_from_current_time();
	}
	else {
		metadata_m.is_sent = false;
		metadata_m.sent_timestamp.reset();
	}
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::is_expired() {
	// check whether we received server ack or not
	time_value curr_time = time_value::get_current_time();

	if (metadata_m.is_sent && !ack_received()) {
		time_value elapsed_from_sent = curr_time.distance(metadata_m.sent_timestamp);

		if (elapsed_from_sent.as_double() > (ACK_TIMEOUT / 1000.0f)) {
			return true;
		}
	}

	// process policy deadlie
	if (metadata_m.policy.deadline > 0.0f) {
		time_value elapsed_from_enqued = curr_time.distance(metadata_m.enqued_timestamp);

		if (elapsed_from_enqued.as_double() > metadata_m.policy.deadline) {
			return true;
		}
	}

	return false;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::remove_from_persistent_cache() {
	return data_m.remove_from_persistent_cache();
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CACHED_MESSAGE_HPP_INCLUDED_
