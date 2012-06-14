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
	code_m(0)
{

}

cached_response_t::cached_response_t(const cached_response_t& response) :
	code_m(0)
{
	*this = response;
}

cached_response_t::cached_response_t(const std::string& uuid,
									 const std::string& route,
									 const message_path& path,
									 const void* data,
									 size_t data_size) :
	uuid_m(uuid),
	route_m(route),
	path_m(path),
	code_m(0)
{
	if (data_size > MAX_RESPONSE_DATA_SIZE) {
		throw dealer_error(resource_error, "can't create response, response data too big.");
	}

	data_m = data_container(data, data_size);
}

cached_response_t::cached_response_t(const std::string& uuid,
									 const std::string& route,
									 const message_path& path,
									 int code,
									 const std::string& error_message) :
	uuid_m(uuid),
	route_m(route),
	path_m(path),
	code_m(code),
	error_message_m(error_message)

{
}

cached_response_t::~cached_response_t() {
}

const data_container&
cached_response_t::data() const {
	return data_m;
}

cached_response_t&
cached_response_t::operator = (const cached_response_t& rhs) {
	boost::mutex::scoped_lock lock(mutex_m);

	if (this == &rhs) {
		return *this;
	}

	uuid_m			= rhs.uuid_m;
	path_m			= rhs.path_m;
	data_m			= rhs.data_m;
	received_timestamp_m = rhs.received_timestamp_m;

	return *this;
}

bool
cached_response_t::operator == (const cached_response_t& rhs) const {
	return (uuid_m == rhs.uuid_m);
}

bool
cached_response_t::operator != (const cached_response_t& rhs) const {
	return !(*this == rhs);
}

const std::string&
cached_response_t::uuid() const {
	return uuid_m;
}

const std::string&
cached_response_t::route() const {
	return route_m;
}

const timeval&
cached_response_t::received_timestamp() const {
	return received_timestamp_m;
}

int
cached_response_t::code() const {
	return code_m;
}

std::string
cached_response_t::error_message() const {
	return error_message_m;
}

void
cached_response_t::set_received_timestamp(const timeval& val) {
	boost::mutex::scoped_lock lock(mutex_m);
	received_timestamp_m = val;
}

void
cached_response_t::set_code(int code) {
	code_m = code;
}

void
cached_response_t::set_error_message(const std::string& message) {
	error_message_m = message;
}

const message_path&
cached_response_t::path() const {
	return path_m;
}

} // namespace dealer
} // namespace cocaine
