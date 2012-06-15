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
	m_code(0)
{

}

cached_response_t::cached_response_t(const cached_response_t& response) :
	m_code(0)
{
	*this = response;
}

cached_response_t::cached_response_t(const std::string& uuid,
									 const std::string& route,
									 const message_path& path,
									 const void* data,
									 size_t data_size) :
	m_uuid(uuid),
	m_route(route),
	m_path(path),
	m_code(0)
{
	if (data_size > MAX_RESPONSE_DATA_SIZE) {
		throw dealer_error(resource_error, "can't create response, response data too big.");
	}

	m_data = data_container(data, data_size);
}

cached_response_t::cached_response_t(const std::string& uuid,
									 const std::string& route,
									 const message_path& path,
									 int code,
									 const std::string& error_message) :
	m_uuid(uuid),
	m_route(route),
	m_path(path),
	m_code(code),
	m_error_message(error_message)

{
}

cached_response_t::~cached_response_t() {
}

const data_container&
cached_response_t::data() const {
	return m_data;
}

cached_response_t&
cached_response_t::operator = (const cached_response_t& rhs) {
	boost::mutex::scoped_lock lock(m_mutex);

	if (this == &rhs) {
		return *this;
	}

	m_uuid			= rhs.m_uuid;
	m_path			= rhs.m_path;
	m_data			= rhs.m_data;
	m_received_timestamp = rhs.m_received_timestamp;

	return *this;
}

bool
cached_response_t::operator == (const cached_response_t& rhs) const {
	return (m_uuid == rhs.m_uuid);
}

bool
cached_response_t::operator != (const cached_response_t& rhs) const {
	return !(*this == rhs);
}

const std::string&
cached_response_t::uuid() const {
	return m_uuid;
}

const std::string&
cached_response_t::route() const {
	return m_route;
}

const timeval&
cached_response_t::received_timestamp() const {
	return m_received_timestamp;
}

int
cached_response_t::code() const {
	return m_code;
}

std::string
cached_response_t::error_message() const {
	return m_error_message;
}

void
cached_response_t::set_received_timestamp(const timeval& val) {
	boost::mutex::scoped_lock lock(m_mutex);
	m_received_timestamp = val;
}

void
cached_response_t::set_code(int code) {
	m_code = code;
}

void
cached_response_t::set_error_message(const std::string& message) {
	m_error_message = message;
}

const message_path&
cached_response_t::path() const {
	return m_path;
}

} // namespace dealer
} // namespace cocaine
