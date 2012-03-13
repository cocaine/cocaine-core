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

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/details/time_value.hpp"
#include "cocaine/dealer/details/message_iface.hpp"
#include "cocaine/dealer/details/error.hpp"
#include "cocaine/dealer/details/progress_timer.hpp"

namespace cocaine {
namespace dealer {

template<typename DataContainer>
class cached_message : public message_iface {
public:
	cached_message();
	explicit cached_message(const cached_message& message);

	cached_message(const message_path& path,
				   const message_policy& policy,
				   const void* data,
				   size_t data_size_);

	~cached_message();

	void* data();
	size_t size() const;

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

private:
	void gen_uuid();
	void init();
	
private:
	// message data
	DataContainer data_;
	message_path path_;
	message_policy policy_;
	std::string uuid_;

	// metadata
	bool is_sent_;
	time_value sent_timestamp_;
	size_t container_size_;
	int timeout_retries_count_;

	// synchronization
	boost::mutex mutex_;
};

template<typename DataContainer>
cached_message<DataContainer>::cached_message() :
	is_sent_(false)
{
	init();
}

template<typename DataContainer>
cached_message<DataContainer>::cached_message(const cached_message& message) {
	*this = message;
}

template<typename DataContainer>
cached_message<DataContainer>::cached_message(const message_path& path,
							   const message_policy& policy,
							   const void* data,
							   size_t data_size) :
	path_(path),
	policy_(policy),
	is_sent_(false),
	container_size_(0)
{
	if (data_size > MAX_MESSAGE_DATA_SIZE) {
		throw error(DEALER_MESSAGE_DATA_TOO_BIG_ERROR, "can't create message, message data too big.");
	}

	data_ = DataContainer(data, data_size);
	init();
}

template<typename DataContainer>
cached_message<DataContainer>::~cached_message() {
}

template<typename DataContainer> void
cached_message<DataContainer>::init() {
	gen_uuid();

	// calc data size
	container_size_ = sizeof(cached_message) + data_.size() + UUID_SIZE + path_.data_size();
}

template<typename DataContainer> void
cached_message<DataContainer>::gen_uuid() {
	char buff[128];
	memset(buff, 0, sizeof(buff));

	uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse(uuid, buff);

	uuid_ = buff;
}

template<typename DataContainer> void*
cached_message<DataContainer>::data() {
	return data_.data();
}

template<typename DataContainer> size_t
cached_message<DataContainer>::size() const {
	return data_.size();
}

template<typename DataContainer> message_iface&
cached_message<DataContainer>::operator = (const message_iface& rhs) {
	boost::mutex::scoped_lock lock(mutex_);

	if (this == &rhs) {
		return *this;
	}

	try {
		const cached_message<DataContainer>& dc = dynamic_cast<const cached_message<DataContainer>& >(rhs);

		data_			= dc.data_;
		path_			= dc.path_;
		policy_			= dc.policy_;
		uuid_			= dc.uuid_;
		is_sent_		= dc.is_sent_;
		sent_timestamp_	= dc.sent_timestamp_;
		container_size_	= dc.container_size_;
	}
	catch (const std::exception& ex) {
		std::string error_msg = ex.what();
		throw error(DEALER_MESSAGE_DATA_TOO_BIG_ERROR, "error in cached_message<DataContainer>::operator = (), details: " + error_msg);
	}

	return *this;
}

template<typename DataContainer> bool
cached_message<DataContainer>::operator == (const message_iface& rhs) const {

	bool comparison_result = false;

	try {
		const cached_message<DataContainer>& dc = dynamic_cast<const cached_message<DataContainer>& >(rhs);
		comparison_result = (uuid_ == dc.uuid_);
	}
	catch (const std::exception& ex) {
		std::string error_msg = ex.what();
		throw error(DEALER_MESSAGE_DATA_TOO_BIG_ERROR, "error in cached_message<DataContainer>::operator = (), details: " + error_msg);
	}

	return comparison_result;
}

template<typename DataContainer> bool
cached_message<DataContainer>::operator != (const message_iface& rhs) const {
	return !(*this == rhs);
}

template<typename DataContainer> const std::string&
cached_message<DataContainer>::uuid() const {
	return uuid_;
}

template<typename DataContainer> bool
cached_message<DataContainer>::is_sent() const {
	return is_sent_;
}

template<typename DataContainer> const time_value&
cached_message<DataContainer>::sent_timestamp() const {
	return sent_timestamp_;
}

template<typename DataContainer> const message_path&
cached_message<DataContainer>::path() const {
	return path_;
}

template<typename DataContainer> const message_policy&
cached_message<DataContainer>::policy() const {
	return policy_;
}

template<typename DataContainer> size_t
cached_message<DataContainer>::container_size() const {
	return container_size_;
}

template<typename DataContainer> void
cached_message<DataContainer>::mark_as_sent(bool value) {
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

template<typename DataContainer> bool
cached_message<DataContainer>::is_expired() {
	if (policy_.deadline == 0.0f) {
		return false;
	}

	if (time_value::get_current_time() > time_value(policy_.deadline)) {
		return true;
	}

	return false;
}

template<typename DataContainer> std::string
cached_message<DataContainer>::json() {
	Json::Value envelope(Json::objectValue);
	Json::FastWriter writer;

	envelope["urgent"] = policy_.urgent;
	envelope["mailboxed"] = policy_.mailboxed;
	envelope["timeout"] = policy_.timeout;
	envelope["deadline"] = policy_.deadline;
	envelope["uuid"] = uuid_;

	return writer.write(envelope);
}

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_CACHED_MESSAGE_HPP_INCLUDED_
