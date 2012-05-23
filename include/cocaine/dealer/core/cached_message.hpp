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

	size_t container_size() const;

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

	std::string json();
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
	// message data
	DataContainer data_;
	MetadataContainer mdata_;

	// synchronization
	boost::mutex mutex_;
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
	mdata_.set_path(path);
	mdata_.policy_ = policy;
	mdata_.enqued_timestamp_.init_from_current_time();

	if (data_size > MAX_MESSAGE_DATA_SIZE) {
		throw dealer_error(resource_error, "can't create message, message data too big.");
	}

	data_.set_data(data, data_size);
	init();
}

template<typename DataContainer, typename MetadataContainer>
cached_message_t<DataContainer, MetadataContainer>::cached_message_t(void* mdata, size_t mdata_size) {
	mdata_.load_data(mdata_, mdata_size);
}

template<typename DataContainer, typename MetadataContainer>
cached_message_t<DataContainer, MetadataContainer>::~cached_message_t() {
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::init() {
	mdata_.uuid_ = wuuid_t().generate();
	mdata_.enqued_timestamp_.init_from_current_time();
}

template<typename DataContainer, typename MetadataContainer> int
cached_message_t<DataContainer, MetadataContainer>::retries_count() const {
	return mdata_.retries_count_;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::increment_retries_count() {
	++mdata_.retries_count_;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::can_retry() const {
	if (mdata_.policy_.max_retries < 0) {
		return true;
	}

	return (mdata_.retries_count_ < mdata_.policy_.max_retries ? true : false);
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::is_data_loaded() {
	return data_.is_data_loaded();
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::load_data() {
	data_.load_data();
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::unload_data() {
	data_.unload_data();
}

template<typename DataContainer, typename MetadataContainer> void*
cached_message_t<DataContainer, MetadataContainer>::data() {
	return data_.data();
}

template<typename DataContainer, typename MetadataContainer> size_t
cached_message_t<DataContainer, MetadataContainer>::size() const {
	return data_.size();
}

template<typename DataContainer, typename MetadataContainer> DataContainer&
cached_message_t<DataContainer, MetadataContainer>::data_container() {
	return data_;
}

template<typename DataContainer, typename MetadataContainer> MetadataContainer&
cached_message_t<DataContainer, MetadataContainer>::mdata_container() {
	return mdata_;
}

template<typename DataContainer, typename MetadataContainer> message_iface&
cached_message_t<DataContainer, MetadataContainer>::operator = (const message_iface& rhs) {
	boost::mutex::scoped_lock lock(mutex_);

	if (this == &rhs) {
		return *this;
	}

	try {
		const cached_message_t<DataContainer, MetadataContainer>& dc = dynamic_cast<const cached_message_t<DataContainer, MetadataContainer>& >(rhs);

		data_			= dc.data_;
		mdata_			= dc.mdata_;
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
		const cached_message_t<DataContainer, MetadataContainer>& dc = dynamic_cast<const cached_message_t<DataContainer, MetadataContainer>& >(rhs);
		comparison_result = (mdata_.uuid_ == dc.mdata_.uuid_);
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
	return mdata_.uuid_;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::is_sent() const {
	return mdata_.is_sent_;
}

template<typename DataContainer, typename MetadataContainer> const time_value&
cached_message_t<DataContainer, MetadataContainer>::sent_timestamp() const {
	return mdata_.sent_timestamp_;
}

template<typename DataContainer, typename MetadataContainer> const time_value&
cached_message_t<DataContainer, MetadataContainer>::enqued_timestamp() const {
	return mdata_.enqued_timestamp_;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::ack_received() const {
	return mdata_.ack_received_;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::set_ack_received(bool value) {
	mdata_.ack_received_ = value;
}

template<typename DataContainer, typename MetadataContainer> const std::string&
cached_message_t<DataContainer, MetadataContainer>::destination_endpoint() const {
	return mdata_.destination_endpoint_;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::set_destination_endpoint(const std::string& value) {
	mdata_.destination_endpoint_ = value;	
}

template<typename DataContainer, typename MetadataContainer> const message_path&
cached_message_t<DataContainer, MetadataContainer>::path() const {
	return mdata_.path();
}

template<typename DataContainer, typename MetadataContainer> const message_policy&
cached_message_t<DataContainer, MetadataContainer>::policy() const {
	return mdata_.policy_;
}

template<typename DataContainer, typename MetadataContainer> size_t
cached_message_t<DataContainer, MetadataContainer>::container_size() const {
	return mdata_.container_size_;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::mark_as_sent(bool value) {
	boost::mutex::scoped_lock lock(mutex_);

	if (value) {
		mdata_.is_sent_ = true;
		mdata_.sent_timestamp_.init_from_current_time();
	}
	else {
		mdata_.is_sent_ = false;
		mdata_.sent_timestamp_.reset();
	}
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message_t<DataContainer, MetadataContainer>::is_expired() {
	// check whether we received server ack or not
	time_value curr_time = time_value::get_current_time();

	if (mdata_.is_sent_ && !ack_received()) {
		time_value elapsed_from_sent = curr_time.distance(mdata_.sent_timestamp_);

		if (elapsed_from_sent.as_double() > (ACK_TIMEOUT / 1000.0f)) {
			return true;
		}
	}

	// process policy deadlie
	if (mdata_.policy_.deadline > 0.0f) {
		time_value elapsed_from_enqued = curr_time.distance(mdata_.enqued_timestamp_);

		if (elapsed_from_enqued.as_double() > mdata_.policy_.deadline) {
			return true;
		}
	}

	return false;
}

template<typename DataContainer, typename MetadataContainer> std::string
cached_message_t<DataContainer, MetadataContainer>::json() {
	Json::Value envelope(Json::objectValue);
	Json::FastWriter writer;

	envelope["urgent"] = mdata_.policy_.urgent;
	envelope["mailboxed"] = mdata_.policy_.mailboxed;
	envelope["timeout"] = mdata_.policy_.timeout;
	envelope["deadline"] = mdata_.policy_.deadline;
	envelope["uuid"] = mdata_.uuid_;

	return writer.write(envelope);
}

template<typename DataContainer, typename MetadataContainer> void
cached_message_t<DataContainer, MetadataContainer>::remove_from_persistent_cache() {
	return data_.remove_from_persistent_cache();
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CACHED_MESSAGE_HPP_INCLUDED_
