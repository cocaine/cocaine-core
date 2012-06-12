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

#include <cstring>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>
#include "json/json.h"

#include "cocaine/dealer/structs.hpp"

#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/utils/error.hpp"

namespace cocaine {
namespace dealer {

cached_response_t::cached_response_t() :
	code_(0)
{

}

cached_response_t::cached_response_t(const cached_response_t& response) :
	code_(0)
{
	*this = response;
}

cached_response_t::cached_response_t(const std::string& uuid,
									 const std::string& route,
									 const message_path& path,
									 const void* data,
									 size_t data_size) :
	uuid_(uuid),
	route_(route),
	path_(path),
	code_(0)
{
	if (data_size > MAX_RESPONSE_DATA_SIZE) {
		throw dealer_error(resource_error, "can't create response, response data too big.");
	}

	data_ = data_container(data, data_size);
}

cached_response_t::cached_response_t(const std::string& uuid,
									 const std::string& route,
									 const message_path& path,
									 int code,
									 const std::string& error_message) :
	uuid_(uuid),
	route_(route),
	path_(path),
	code_(code),
	error_message_(error_message)

{
}

cached_response_t::~cached_response_t() {
}

const data_container&
cached_response_t::data() const {
	return data_;
}

cached_response_t&
cached_response_t::operator = (const cached_response_t& rhs) {
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
cached_response_t::operator == (const cached_response_t& rhs) const {
	return (uuid_ == rhs.uuid_);
}

bool
cached_response_t::operator != (const cached_response_t& rhs) const {
	return !(*this == rhs);
}

const std::string&
cached_response_t::uuid() const {
	return uuid_;
}

const std::string&
cached_response_t::route() const {
	return route_;
}

const timeval&
cached_response_t::received_timestamp() const {
	return received_timestamp_;
}

int
cached_response_t::code() const {
	return code_;
}

std::string
cached_response_t::error_message() const {
	return error_message_;
}

void
cached_response_t::set_received_timestamp(const timeval& val) {
	boost::mutex::scoped_lock lock(mutex_);
	received_timestamp_ = val;
}

void
cached_response_t::set_code(int code) {
	code_ = code;
}

void
cached_response_t::set_error_message(const std::string& message) {
	error_message_ = message;
}

const message_path&
cached_response_t::path() const {
	return path_;
}

size_t
cached_response_t::container_size() const {
	return container_size_;
}

} // namespace dealer
} // namespace cocaine
