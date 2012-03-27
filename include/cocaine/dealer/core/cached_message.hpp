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
#include <stdexcept>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>
#include "json/json.h"

#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"

namespace cocaine {
namespace dealer {

template<typename DataContainer, typename MetadataContainer>
class cached_message : public message_iface {
public:
	cached_message();
	explicit cached_message(const cached_message& message);

	cached_message(const message_path& path,
				   const message_policy& policy,
				   const void* data,
				   size_t data_size);

	cached_message(void* mdata,
				   size_t mdata_size);

	~cached_message();

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

	void mark_as_sent(bool value);

	std::string json();
	bool is_expired();

	message_iface& operator = (const message_iface& rhs);
	bool operator == (const message_iface& rhs) const;
	bool operator != (const message_iface& rhs) const;

	bool is_data_loaded();
	void load_data();
	void unload_data();

	void remove_from_persistent_cache();

private:
	void gen_uuid();
	void init();
	
private:
	// message data
	DataContainer data_;
	MetadataContainer mdata_;

	// ivars
	time_value sent_timestamp_;
	bool is_sent_;
	size_t container_size_;
	int timeout_retries_count_;

	// synchronization
	boost::mutex mutex_;
};

template<typename DataContainer, typename MetadataContainer>
cached_message<DataContainer, MetadataContainer>::cached_message() :
	is_sent_(false)
{
	init();
}

template<typename DataContainer, typename MetadataContainer>
cached_message<DataContainer, MetadataContainer>::cached_message(const cached_message& message) {
	*this = message;
}

template<typename DataContainer, typename MetadataContainer>
cached_message<DataContainer, MetadataContainer>::cached_message(const message_path& path,
							   									 const message_policy& policy,
							   									 const void* data,
							   									 size_t data_size) :
	is_sent_(false),
	container_size_(0)
{
	mdata_.path = path;
	mdata_.policy = policy;
	mdata_.enqueued_timestamp.init_from_current_time();

	if (data_size > MAX_MESSAGE_DATA_SIZE) {
		throw error(DEALER_MESSAGE_DATA_TOO_BIG_ERROR, "can't create message, message data too big.");
	}

	data_.set_data(data, data_size);
	init();
}

template<typename DataContainer, typename MetadataContainer>
cached_message<DataContainer, MetadataContainer>::cached_message(void* mdata, size_t mdata_size) {
	mdata_.load_data(mdata_, mdata_size);
}

template<typename DataContainer, typename MetadataContainer>
cached_message<DataContainer, MetadataContainer>::~cached_message() {
}

template<typename DataContainer, typename MetadataContainer> void
cached_message<DataContainer, MetadataContainer>::init() {
	gen_uuid();
	mdata_.enqueued_timestamp.init_from_current_time();

	// calc data size
	container_size_ = sizeof(cached_message) + data_.size() + UUID_SIZE + mdata_.path.data_size();
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message<DataContainer, MetadataContainer>::is_data_loaded() {
	return data_.is_data_loaded();
}

template<typename DataContainer, typename MetadataContainer> void
cached_message<DataContainer, MetadataContainer>::load_data() {
	data_.load_data();
}

template<typename DataContainer, typename MetadataContainer> void
cached_message<DataContainer, MetadataContainer>::unload_data() {
	data_.unload_data();
}

template<typename DataContainer, typename MetadataContainer> void
cached_message<DataContainer, MetadataContainer>::gen_uuid() {
	char buff[128];
	memset(buff, 0, sizeof(buff));

	uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse(uuid, buff);

	mdata_.uuid = buff;
}

template<typename DataContainer, typename MetadataContainer> void*
cached_message<DataContainer, MetadataContainer>::data() {
	return data_.data();
}

template<typename DataContainer, typename MetadataContainer> size_t
cached_message<DataContainer, MetadataContainer>::size() const {
	return data_.size();
}

template<typename DataContainer, typename MetadataContainer> DataContainer&
cached_message<DataContainer, MetadataContainer>::data_container() {
	return data_;
}

template<typename DataContainer, typename MetadataContainer> MetadataContainer&
cached_message<DataContainer, MetadataContainer>::mdata_container() {
	return mdata_;
}

template<typename DataContainer, typename MetadataContainer> message_iface&
cached_message<DataContainer, MetadataContainer>::operator = (const message_iface& rhs) {
	boost::mutex::scoped_lock lock(mutex_);

	if (this == &rhs) {
		return *this;
	}

	try {
		const cached_message<DataContainer, MetadataContainer>& dc = dynamic_cast<const cached_message<DataContainer, MetadataContainer>& >(rhs);

		data_			= dc.data_;
		mdata_			= dc.mdata_;
		is_sent_		= dc.is_sent_;
		sent_timestamp_	= dc.sent_timestamp_;
		container_size_	= dc.container_size_;
	}
	catch (const std::exception& ex) {
		std::string error_msg = ex.what();
		throw error(DEALER_MESSAGE_DATA_TOO_BIG_ERROR, "error in cached_message<DataContainer, MetadataContainer>::operator = (), details: " + error_msg);
	}

	return *this;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message<DataContainer, MetadataContainer>::operator == (const message_iface& rhs) const {

	bool comparison_result = false;

	try {
		const cached_message<DataContainer, MetadataContainer>& dc = dynamic_cast<const cached_message<DataContainer, MetadataContainer>& >(rhs);
		comparison_result = (mdata_.uuid == dc.mdata_.uuid);
	}
	catch (const std::exception& ex) {
		std::string error_msg = ex.what();
		throw error(DEALER_MESSAGE_DATA_TOO_BIG_ERROR, "error in cached_message<DataContainer, MetadataContainer>::operator = (), details: " + error_msg);
	}

	return comparison_result;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message<DataContainer, MetadataContainer>::operator != (const message_iface& rhs) const {
	return !(*this == rhs);
}

template<typename DataContainer, typename MetadataContainer> const std::string&
cached_message<DataContainer, MetadataContainer>::uuid() const {
	return mdata_.uuid;
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message<DataContainer, MetadataContainer>::is_sent() const {
	return is_sent_;
}

template<typename DataContainer, typename MetadataContainer> const time_value&
cached_message<DataContainer, MetadataContainer>::sent_timestamp() const {
	return sent_timestamp_;
}

template<typename DataContainer, typename MetadataContainer> const message_path&
cached_message<DataContainer, MetadataContainer>::path() const {
	return mdata_.path;
}

template<typename DataContainer, typename MetadataContainer> const message_policy&
cached_message<DataContainer, MetadataContainer>::policy() const {
	return mdata_.policy;
}

template<typename DataContainer, typename MetadataContainer> size_t
cached_message<DataContainer, MetadataContainer>::container_size() const {
	return container_size_;
}

template<typename DataContainer, typename MetadataContainer> void
cached_message<DataContainer, MetadataContainer>::mark_as_sent(bool value) {
	boost::mutex::scoped_lock lock(mutex_);

	if (value) {
		is_sent_ = false;
		sent_timestamp_.init_from_current_time();
	}
	else {
		is_sent_ = false;
		sent_timestamp_.reset();
	}
}

template<typename DataContainer, typename MetadataContainer> bool
cached_message<DataContainer, MetadataContainer>::is_expired() {
	if (mdata_.policy.deadline == 0.0f) {
		return false;
	}

	if (time_value::get_current_time() > time_value(mdata_.policy.deadline)) {
		return true;
	}

	return false;
}

template<typename DataContainer, typename MetadataContainer> std::string
cached_message<DataContainer, MetadataContainer>::json() {
	Json::Value envelope(Json::objectValue);
	Json::FastWriter writer;

	envelope["urgent"] = mdata_.policy.urgent;
	envelope["mailboxed"] = mdata_.policy.mailboxed;
	envelope["timeout"] = mdata_.policy.timeout;
	envelope["deadline"] = mdata_.policy.deadline;
	envelope["uuid"] = mdata_.uuid;

	return writer.write(envelope);
}

template<typename DataContainer, typename MetadataContainer> void
cached_message<DataContainer, MetadataContainer>::remove_from_persistent_cache() {
	return data_.remove_from_persistent_cache();
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CACHED_MESSAGE_HPP_INCLUDED_
