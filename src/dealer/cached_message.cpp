//
// Copyright (C) 2011 Rim Zaidullin <creator@bash.org.ru>
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

#include <cstring>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>
#include "json/json.h"

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/details/cached_message.hpp"
#include "cocaine/dealer/details/error.hpp"
#include "cocaine/dealer/details/progress_timer.hpp"

namespace lsd {
cached_message::cached_message() :
	is_sent_(false)
{
	init();
}

cached_message::cached_message(const cached_message& message) {
	*this = message;
}

cached_message::cached_message(const message_path& path,
							   const message_policy& policy,
							   const void* data,
							   size_t data_size) :
	path_(path),
	policy_(policy),
	is_sent_(false),
	container_size_(0)
{
	if (data_size > MAX_MESSAGE_DATA_SIZE) {
		throw error(LSD_MESSAGE_DATA_TOO_BIG_ERROR, "can't create message, message data too big.");
	}

	data_ = data_container(data, data_size);
	init();
}

cached_message::~cached_message() {
}

void
cached_message::init() {
	gen_uuid();

	// calc data size
	container_size_ = sizeof(cached_message) + data_.size() + UUID_SIZE + path_.container_size();
}

void
cached_message::gen_uuid() {
	char buff[128];
	memset(buff, 0, sizeof(buff));

	uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse(uuid, buff);

	uuid_ = buff;
}

const data_container&
cached_message::data() const {
	return data_;
}

cached_message&
cached_message::operator = (const cached_message& rhs) {
	boost::mutex::scoped_lock lock(mutex_);

	if (this == &rhs) {
		return *this;
	}

	data_			= rhs.data_;
	path_			= rhs.path_;
	policy_			= rhs.policy_;
	uuid_			= rhs.uuid_;
	is_sent_		= rhs.is_sent_;
	sent_timestamp_	= rhs.sent_timestamp_;
	container_size_	= rhs.container_size_;

	return *this;
}

bool
cached_message::operator == (const cached_message& rhs) const {
	return (uuid_ == rhs.uuid_);
}

bool
cached_message::operator != (const cached_message& rhs) const {
	return !(*this == rhs);
}

const std::string&
cached_message::uuid() const {
	return uuid_;
}

bool
cached_message::is_sent() const {
	return is_sent_;
}

const time_value&
cached_message::sent_timestamp() const {
	return sent_timestamp_;
}

const message_path&
cached_message::path() const {
	return path_;
}

const message_policy&
cached_message::policy() const {
	return policy_;
}

size_t
cached_message::container_size() const {
	return container_size_;
}

void
cached_message::mark_as_sent(bool value) {
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

bool
cached_message::is_expired() {
	if (policy_.deadline == 0.0f) {
		return false;
	}

	if (time_value::get_current_time() > time_value(policy_.deadline)) {
		return true;
	}

	return false;
}

std::string
cached_message::json() {
	Json::Value envelope(Json::objectValue);
	Json::FastWriter writer;

	envelope["urgent"] = policy_.urgent;
	envelope["mailboxed"] = policy_.mailboxed;
	envelope["timeout"] = policy_.timeout;
	envelope["deadline"] = policy_.deadline;
	envelope["uuid"] = uuid_;

	return writer.write(envelope);
}

} // namespace lsd
