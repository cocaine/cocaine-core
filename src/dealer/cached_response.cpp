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

#include <cstring>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>
#include "json/json.h"

#include "cocaine/dealer/structs.hpp"

#include "cocaine/dealer/details/cached_response.hpp"
#include "cocaine/dealer/details/error.hpp"

namespace cocaine {
namespace dealer {

cached_response::cached_response() :
	error_code_(0)
{

}

cached_response::cached_response(const cached_response& response) :
	error_code_(0)
{
	*this = response;
}

cached_response::cached_response(const std::string& uuid,
								 const message_path& path,
								 const void* data,
								 size_t data_size) :
	uuid_(uuid),
	path_(path),
	error_code_(0)
{
	if (data_size > MAX_RESPONSE_DATA_SIZE) {
		throw error(DEALER_MESSAGE_DATA_TOO_BIG_ERROR, "can't create response, response data too big.");
	}

	data_ = data_container(data, data_size);

	// calc data size
	container_size_ = sizeof(cached_response) + data_.size() + UUID_SIZE;
	container_size_ += path_.data_size() + error_message_.length();
}

cached_response::cached_response(const std::string& uuid,
								 const message_path& path,
								 int error_code,
								 const std::string& error_message) :
	uuid_(uuid),
	path_(path),
	error_code_(error_code),
	error_message_(error_message)

{
	container_size_ = sizeof(cached_response) + data_.size() + UUID_SIZE;
	container_size_ += path_.data_size() + error_message_.length();
}

cached_response::~cached_response() {
}

const data_container&
cached_response::data() const {
	return data_;
}

cached_response&
cached_response::operator = (const cached_response& rhs) {
	boost::mutex::scoped_lock lock(mutex_);

	if (this == &rhs) {
		return *this;
	}

	uuid_			= rhs.uuid_;
	path_			= rhs.path_;
	data_			= rhs.data_;
	received_timestamp_	= rhs.received_timestamp_;
	container_size_		= rhs.container_size_;

	return *this;
}

bool
cached_response::operator == (const cached_response& rhs) const {
	return (uuid_ == rhs.uuid_);
}

bool
cached_response::operator != (const cached_response& rhs) const {
	return !(*this == rhs);
}

const std::string&
cached_response::uuid() const {
	return uuid_;
}

const timeval&
cached_response::received_timestamp() const {
	return received_timestamp_;
}

int
cached_response::error_code() const {
	return error_code_;
}

std::string
cached_response::error_message() const {
	return error_message_;
}

void
cached_response::set_received_timestamp(const timeval& val) {
	boost::mutex::scoped_lock lock(mutex_);
	received_timestamp_ = val;
}

void
cached_response::set_error(int code, const std::string& message) {
	error_code_ = code;
	error_message_ = message;
}

const message_path&
cached_response::path() const {
	return path_;
}

size_t
cached_response::container_size() const {
	return container_size_;
}

} // namespace dealer
} // namespace cocaine
