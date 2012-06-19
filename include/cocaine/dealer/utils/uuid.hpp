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

#ifndef _COCAINE_DEALER_UUID_HPP_INCLUDED_
#define _COCAINE_DEALER_UUID_HPP_INCLUDED_

#include <uuid/uuid.h>

namespace cocaine {
namespace dealer {

class wuuid_t {
public:
	wuuid_t() {
		clear();
	}

	wuuid_t(const wuuid_t& rhs) :
		uuid_string_(rhs.uuid_string_)
	{
		uuid_copy(uuid_data_, rhs.uuid_data_);
	}

	wuuid_t(const uuid_t& rhs) {
		uuid_copy(uuid_data_, rhs);
		generate_string();
	}

	wuuid_t(const std::string& uuid_string) {
		if (0 == uuid_parse(uuid_string.c_str(), uuid_data_)) {
			uuid_string_ = uuid_string;
		}
		else {
			clear();
		}
	}

	wuuid_t(const char* uuid_string) {
		if (0 == uuid_parse(uuid_string, uuid_data_)) {
			uuid_string_ = uuid_string;
		}
		else {
			clear();
		}
	}

	~wuuid_t() {}

	bool is_null() const {
		return (uuid_is_null(uuid_data_) == 1 ? true : false);
	}

	void clear() {
		uuid_clear(uuid_data_);
		generate_string();
	}

	const std::string& generate() {
		generate_data();
		generate_string();
		return uuid_string_;
	}

	const std::string& str() const {
		return uuid_string_;
	}

	const uuid_t& data() const {
		return uuid_data_;
	}

	bool operator == (const wuuid_t& rhs) const {
		return (uuid_compare(uuid_data_, rhs.uuid_data_) == 0 ? true : false);
	}

	bool operator != (const wuuid_t& rhs) const {
		return (!(*this == rhs));
	}

	bool operator == (const std::string& rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this == tmp_rhs);
	}

	bool operator != (const std::string& rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this != tmp_rhs);
	}

	bool operator == (const char* rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this == tmp_rhs);
	}

	bool operator != (const char* rhs) const {
		wuuid_t tmp_rhs(rhs);
		return (*this != tmp_rhs);
	}

private:
	void generate_data() {
		uuid_generate(uuid_data_);
	}

	void generate_string() {
		char buff[37];
		memset(buff, 0, sizeof(buff));
		uuid_unparse(uuid_data_, buff);

		uuid_string_ = buff;
	}

	uuid_t uuid_data_;
	std::string uuid_string_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_UUID_HPP_INCLUDED_
